"""Seed-time inland climate mask; no chunks, online flood fill, or fixed coordinates."""
from pathlib import Path
from world_bounds import HALF
PAD=((HALF+2048+63)//64)*64
GRID=PAD//32
TILE=PAD//64
import json, math, heapq, hashlib, subprocess
import numpy as np
P=Path(__file__).resolve().parents[1]

def smooth(a,b,v):
    t=np.clip((v-a)/(b-a),0,1);return t*t*(3-2*t)

def large_regions(d,special,volcanic,X,Z,seed):
    """Qualify connected sandy cores, never the faint transition envelope."""
    w=smooth(420,980,d)*special*(1-smooth(.01,.20,volcanic))
    inside=(np.abs(X)<=(HALF-192))&(np.abs(Z)<=(HALF-192))
    todo=set(map(tuple,np.argwhere((w>=.70)&inside)))
    qualified=[];rejected=[]
    while todo:
        group=[todo.pop()]
        for z,x in group:
            for dz,dx in [(0,1),(0,-1),(1,0),(-1,0)]:
                q=z+dz,x+dx
                if q in todo:todo.remove(q);group.append(q)
        zz,xx=np.array(group).T
        span=[int(np.ptp(xx))*64,int(np.ptp(zz))*64]
        area=len(group)*4096
        record=dict(coreSpan=span,coreArea=area,coreSamples=len(group))
        # More than 100 chunks on BOTH axes, and substantial occupied area;
        # a thin L-shaped corridor cannot pass by bounding box alone.
        if min(span)>=1664 and area>=1664**2*.65:
            qualified.append((group,record))
        elif area>65536:rejected.append(record)
    if not qualified:return np.zeros(w.shape),[0,0],dict(accepted=False,rejected=rejected,regions=[])
    def rank(item):
        group,record=item;z,x=min(group)
        noise=((seed^(x*73856093)^(z*19349663))&0xffff)/65535
        return record['coreArea']*(.95+.1*noise)
    group,record=max(qualified,key=rank)
    # Grow only a short, continuous margin from a qualified core. Unrelated
    # inland islands and old climate-classifier deserts do not sneak back in.
    dist=np.full(w.shape,1e9);heap=[]
    for z,x in group:dist[z,x]=0;heap.append((0.,z,x))
    heapq.heapify(heap)
    while heap:
        v,z,x=heapq.heappop(heap)
        if v!=dist[z,x] or v>=448:continue
        for dz,dx,c in [(0,1,64),(0,-1,64),(1,0,64),(-1,0,64),(1,1,math.sqrt(2)*64),(1,-1,math.sqrt(2)*64),(-1,1,math.sqrt(2)*64),(-1,-1,math.sqrt(2)*64)]:
            zz,xx=z+dz,x+dx
            if 0<=zz<GRID and 0<=xx<GRID and w[zz,xx]>.02 and v+c<dist[zz,xx]:
                dist[zz,xx]=v+c;heapq.heappush(heap,(v+c,zz,xx))
    w*=1-smooth(128,448,dist)
    # Choose an interior point, not an edge-biased highest-coast-distance node.
    mx=np.mean([X[q] for q in group]);mz=np.mean([Z[q] for q in group])
    z,x=min(group,key=lambda k:(X[k]-mx)**2+(Z[k]-mz)**2)
    return w,[int(X[z,x]),int(Z[z,x])],dict(accepted=True,rejected=rejected,regions=[record],minSpanBlocks=1664,minCoreArea=1664**2*.65)

def scan_climate(exe,base,out,progress=None):
    # Preserve the exact 64-block sample lattice. Small completed tiles expose
    # real progress rather than one opaque scan of the whole world.
    from concurrent.futures import ThreadPoolExecutor, as_completed
    import threading
    cells=max(n for n in range(1,min(80,TILE)+1) if TILE%n==0)
    width=cells*64
    jobs=[(x,z) for z in range(-PAD,PAD,width) for x in range(-PAD,PAD,width)]
    a=np.zeros((GRID,GRID,10))
    local=threading.local();processes=[]
    def start_worker():
        # Each worker retains its own immutable native field. Avoid reloading
        # the entire seed for every small tile.
        local.process=subprocess.Popen([str(exe),str(base)],stdin=subprocess.PIPE,
                                       stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        processes.append(local.process)
    def tile(q):
        x,z=q;path=out/f'desert-coarse-{x}-{z}.json'
        query=dict(scan=True,x=x,z=z,width=width,step=64,output=str(path))
        process=local.process
        process.stdin.write(json.dumps(query)+'\n');process.stdin.flush()
        response=process.stdout.readline()
        if not response:raise RuntimeError('Climate scan failed: '+process.stderr.read())
        if json.loads(response).get('samples')!=cells*cells:raise ValueError('Climate scan sample count mismatch')
        return x,z,np.array(json.loads(path.read_text())).reshape(cells,cells,10)
    try:
        with ThreadPoolExecutor(max_workers=2,initializer=start_worker) as pool:
            pending=[pool.submit(tile,q) for q in jobs]
            for done,f in enumerate(as_completed(pending),1):
                x,z,t=f.result();iz=(z+PAD)//64;ix=(x+PAD)//64
                a[iz:iz+cells,ix:ix+cells]=t
                if progress:progress(f'分析沙漠与气候：{done}/{len(jobs)}',.85*done/len(jobs))
    finally:
        for process in processes:
            if process.poll() is None:
                process.stdin.close()
                try:process.wait(timeout=5)
                except subprocess.TimeoutExpired:process.kill();process.wait()
            process.stdout.close();process.stderr.close()
    return a

def bake(source,out,progress=None):
    out=Path(out);out.mkdir(parents=True,exist_ok=True)
    j=json.loads(Path(source).read_text('utf-8'));j.pop('aridLandforms',None)
    if not j['config'].get('deserts',{}).get('enabled',False):return
    # A 2048-block halo includes sea beyond the test boundary; that boundary
    # is not treated as coastline. Bounded tiles retain the original samples.
    sample=json.loads(json.dumps(j));sample.pop('deserts',None)
    sample['naturalLakes']=[]
    for k in ['rivers','overhangCliffs','riverSource','naturalLakeSource']:sample.pop(k,None)
    base=out/'desert-base.json';base.write_text(json.dumps(sample,separators=(',',':')))
    exe=P/'build/windows/x64/release/native-worldgen-check.exe'
    a=scan_climate(exe,base,out,progress)
    if progress:progress('正在计算距海距离与沙漠范围',.85)
    h=a[:,:,2];d=np.full(h.shape,np.inf);heap=[]
    for z,x in zip(*np.where(h<31)):d[z,x]=0;heap.append((0.,int(z),int(x)))
    heapq.heapify(heap)
    while heap:
        v,z,x=heapq.heappop(heap)
        if v!=d[z,x]:continue
        for dz,dx,c in [(0,1,64),(0,-1,64),(1,0,64),(-1,0,64),(1,1,math.sqrt(2)*64),(1,-1,math.sqrt(2)*64),(-1,1,math.sqrt(2)*64),(-1,-1,math.sqrt(2)*64)]:
            zz,xx=z+dz,x+dx
            if 0<=zz<GRID and 0<=xx<GRID and v+c<d[zz,xx]:d[zz,xx]=v+c;heapq.heappush(heap,(v+c,zz,xx))
    # Chamfer distances can overestimate Euclidean sea distance. This margin
    # also accounts for the 64-block shoreline sampling uncertainty.
    d=np.maximum(0,d*.92-90)
    X=a[:,:,0];Z=a[:,:,1];seed=j['config']['seed']
    special=np.ones(h.shape)
    for name in ['giantForest','stoneForest']:
        c=j['config'][name];r=c['outerRadius']+c.get('edgeWarp',0)
        special*=smooth(r+100,r+480,np.hypot(X-c['centerX'],Z-c['centerZ']))
    eligible=(np.abs(X)<(HALF-446))&(np.abs(Z)<(HALF-446))&(h>42)&(h<170)&(a[:,:,9]==0)&(special>.99)&(d>650)
    candidates=np.argwhere(eligible)
    def score(k):
        z,x=map(int,k);phase=(seed%100003)*.00013
        return d[z,x]+90*math.sin(x*.081+phase)*math.cos(z*.067-phase)-.45*max(0,h[z,x]-100)
    qualification=None
    if j['config']['deserts'].get('largeSandOnly',False):
        w,center,qualification=large_regions(d,special,a[:,:,9],X,Z,seed)
        cx,cz=center;angle=((seed%100003)*.00013)%math.tau
    elif len(candidates):
        z,x=map(int,max(candidates,key=score));cx,cz=int(X[z,x]),int(Z[z,x])
        phase=(seed%100003)*.00013;angle=phase%math.tau
        dx=X-cx;dz=Z-cz;u=dx*math.cos(angle)+dz*math.sin(angle);v=-dx*math.sin(angle)+dz*math.cos(angle)
        radius=np.hypot(u/2100,v/1650)
        warped=radius+.065*np.sin(X/380+phase)*np.cos(Z/470-phase)
        w=(1-smooth(.40,1.,warped))*smooth(450,1050,d)*special
        w*=1-smooth(.01,.20,a[:,:,9])
    else:cx=cz=0;angle=0.;w=np.zeros(h.shape)
    # Outside the sampled halo, fade to zero continuously; never crop at the
    # +/-4096 playable edge or paint a bridge across existing sea.
    w*=1-smooth(PAD-644,PAD-192,np.maximum(np.abs(X),np.abs(Z)))
    quant=np.rint(w*255).astype(int)
    desc=dict(algorithm='inland-arid-mask-v1',seed=seed,origin=-PAD,pitch=64,side=GRID,
              weights=quant.flatten().tolist(),wind=angle,center=[cx,cz],
              coastPolicy=dict(start=450,full=1050,halo=2048,conservative=True))
    if qualification is not None:
        desc.update(algorithm='large-inland-sand-v2',qualification=qualification,coastPolicy=dict(start=420,full=980,halo=2048,conservative=True))
    j['deserts']=desc
    Path(source).write_text(json.dumps(j,separators=(',',':')),encoding='utf-8')
    inside=(np.abs(X)<=HALF)&(np.abs(Z)<=HALF);core=inside&(w>=.65);belt=inside&(w>=.15)
    report=dict(center=[cx,cz],terrainSeed=j['seedManifest']['terrainSeed'],
                coreSamples=int(core.sum()),beltAreaBlocks=int(belt.sum()*4096),
                coreMinSeaDistance=float(d[core].min()) if core.any() else None,
                beltSpan=[int(np.ptp(X[belt]))+64,int(np.ptp(Z[belt]))+64] if belt.any() else [0,0],
                possible=bool(np.any(quant)),qualification=qualification,maskSha256=hashlib.sha256(quant.astype(np.uint8).tobytes()).hexdigest())
    (out/'desert-plan.json').write_text(json.dumps(report,indent=2))
    np.savez_compressed(out/'desert-analysis.npz',height=h,seaDistance=d,weight=w)
    print(json.dumps({'desert':report}),flush=True)
    if progress:progress('沙漠与气候分析完成',1.)

if __name__=='__main__':
    import argparse
    a=argparse.ArgumentParser();a.add_argument('--source',type=Path,required=True);a.add_argument('--output',type=Path,required=True);v=a.parse_args();bake(v.source,v.output)
