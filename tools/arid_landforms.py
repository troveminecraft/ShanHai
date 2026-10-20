"""Bounded seed-time dry drainage, piedmont fans and closed-basin playas.

Only sparse dry-ground spans are frozen. Existing wet rivers, lake banks and
desert masks are inputs, never rewritten; runtime needs no drainage solver.
"""
from pathlib import Path
from world_bounds import HALF
import argparse, collections, hashlib, heapq, json, math, subprocess
import numpy as np
from bake_natural_lakes import fill
from fluvial_landforms import encode, tube, rnd

P=Path(__file__).resolve().parents[1]
E=P/'build/windows/x64/release/native-worldgen-check.exe'

def digest(j):return hashlib.sha256(json.dumps(j,sort_keys=True,separators=(',',':')).encode()).hexdigest()
def identity(j):return digest({k:v for k,v in j.items() if k not in ('lavaFlows','iceRegions','version','aridLandforms','cavernPlan','stoneForestGrotto')})
def smooth(t):
    t=np.clip(t,0,1);return t*t*(3-2*t)
def weights(j,xx,zz):
    d=j['deserts'];a=np.array(d['weights']).reshape(d['side'],d['side'])/255
    x=(xx-d['origin'])/d['pitch'];z=(zz-d['origin'])/d['pitch']
    ix=np.floor(x).astype(int);iz=np.floor(z).astype(int);u=smooth(x-ix);v=smooth(z-iz)
    ix=np.clip(ix,0,d['side']-2);iz=np.clip(iz,0,d['side']-2)
    return (a[iz,ix]*(1-u)+a[iz,ix+1]*u)*(1-v)+(a[iz+1,ix]*(1-u)+a[iz+1,ix+1]*u)*v
def dilate(a,n):
    out=a.copy()
    for _ in range(n):
        b=out.copy();b[1:]|=out[:-1];b[:-1]|=out[1:];b[:,1:]|=out[:,:-1];b[:,:-1]|=out[:,1:];out=b
    return out
def components(mask):
    left=set(map(tuple,np.argwhere(mask)));groups=[]
    while left:
        q=[min(left)];left.remove(q[0])
        for z,x in q:
            for k in [(z+1,x),(z-1,x),(z,x+1),(z,x-1)]:
                if k in left:left.remove(k);q.append(k)
        groups.append(q)
    return sorted(groups,key=lambda q:(-len(q),q[0]))

def fan_visibility(height,deposit,along,across,length,apex_y,grade):
    """A fan must survive the existing dunes as a broad visible apron."""
    mask=deposit>0;area=int(mask.sum())
    if area<1200:return False,'fan-too-small'
    if np.count_nonzero(deposit>=2)<max(120,area*.20):return False,'fan-buried-by-existing-relief'
    width=float(np.ptp(across[mask]));span=float(np.ptp(along[mask]))
    if width<max(40,length*.38) or span<length*.50:return False,'fan-narrow-fragment'
    if min(np.count_nonzero(mask&(across<0)),np.count_nonzero(mask&(across>0)))<area*.18:return False,'fan-one-sided-fragment'
    residual=height-(apex_y-grade*along)
    if np.percentile(residual[mask],90)-np.percentile(residual[mask],10)>5:return False,'fan-dune-relief-too-large'
    return True,None

def bake(source,out):
    source=Path(source);out=Path(out);out.mkdir(parents=True,exist_ok=True)
    j=json.loads(source.read_text('utf-8'));j.pop('aridLandforms',None)
    base=out/'arid-base-input.json';base.write_text(json.dumps(j,separators=(',',':')),'utf-8')
    seed=j['config']['seed'];features=[];used=set();rejected=collections.Counter()
    def scan(x,z,n,step,label):
        path=out/f'{label}.json'
        q=dict(scan=True,x=int(x),z=int(z),width=int(n),step=int(step),output=str(path))
        subprocess.run([str(E),str(base)],input=json.dumps(q)+'\n',text=True,capture_output=True,check=True)
        return np.array(json.loads(path.read_text())).reshape(math.ceil(n/step),math.ceil(n/step),10)
    def valid(a,w):
        return (w>.45)&(a[:,:,2]>40)&(a[:,:,2]<210)&(a[:,:,3]<a[:,:,2])&(a[:,:,5]==0)&(a[:,:,6]<.01)&(a[:,:,7]<.01)&(a[:,:,8]==-32768)&(a[:,:,9]==0)
    def add(kind,cells,**info):
        if not cells or any(k in used for k in cells):return False
        if sum(len(f['rows']) for f in features)+len(cells)>180000:return False
        used.update(cells);features.append(dict(kind=kind,rows=encode(cells),**info));return True
    if j.get('deserts',{}).get('qualification',{}).get('accepted'):
        d=j['deserts'];mask=np.array(d['weights']).reshape(d['side'],d['side'])>.45*255
        zz,xx=np.where(mask);x0=max((-HALF+208),int(xx.min()*d['pitch']+d['origin'])-160);z0=max((-HALF+208),int(zz.min()*d['pitch']+d['origin'])-160)
        n=min((2*HALF-416),max(int(xx.max()*d['pitch']+d['origin'])-x0,int(zz.max()*d['pitch']+d['origin'])-z0)+192)
        n=min(n,HALF-208-x0,HALF-208-z0);n=(n//16)*16
        a=scan(x0,z0,n,16,'arid-coarse');h=a[:,:,2].astype(int);X=a[:,:,0];Z=a[:,:,1]
        w=weights(j,X,Z);ok=valid(a,w);f=fill(h)
        # A closed depression must occupy a broad real basin, not a single dune pit.
        groups=components((f-h>=2)&ok)
        for group in groups:
            if sum(v['kind']=='playa' for v in features)>=2:break
            if len(group)<16:continue
            q=min(group,key=lambda k:(h[k],k));cx,cz=int(X[q]),int(Z[q])
            if not(-3720<cx<3720 and -3720<cz<3720):continue
            if any(math.dist([cx,cz],v['centre'])<360 for v in features):continue
            b=scan(cx-144,cz-144,289,1,f'playa-{cx}-{cz}');H=b[:,:,2].astype(int);B=fill(H);W=weights(j,b[:,:,0],b[:,:,1])
            candidates=np.where((B-H>=3)&valid(b,W),H,999)
            iz,ix=map(int,np.unravel_index(candidates.argmin(),H.shape))
            if candidates[iz,ix]==999:rejected['no-closed-dry-basin']+=1;continue
            floor=int(H[iz,ix]);spill=int(B[iz,ix]);level=min(spill-1,floor+4)
            todo=[(iz,ix)];seen={(iz,ix)};escaped=False
            for z,x in todo:
                if z in (0,288) or x in (0,288):escaped=True;break
                for k in [(z+1,x),(z-1,x),(z,x+1),(z,x-1)]:
                    if k not in seen and H[k]<level:seen.add(k);todo.append(k)
            region=np.zeros(H.shape,bool)
            for k in seen:region[k]=True
            halo=dilate(region,20)
            if escaped or not 1200<=len(seen)<=24000 or not np.all(valid(b,W)[halo]):rejected['playa-shape-or-water-buffer']+=1;continue
            ys,xs=np.where(region)
            if min(np.ptp(xs),np.ptp(ys))<48:rejected['playa-too-narrow']+=1;continue
            # Dry sediment settles in the existing bowl; maximum two blocks
            # deposition, no basin excavation or raised rim, no added water.
            target=np.minimum(H+2,np.maximum(H,level-2));target=np.where(region,target,H)
            inner=region&~dilate(~region,5);core=inner&(H<=floor+1)
            # Irregular Voronoi drying seams, computed once, not a tiled grid.
            XX=b[:,:,0];ZZ=b[:,:,1];near=np.full(H.shape,1e9);second=near.copy()
            for gz in range((cz-144)//24-1,(cz+144)//24+2):
                for gx in range((cx-144)//24-1,(cx+144)//24+2):
                    px=(gx+.12+.76*rnd(seed,'crack-x',gx,gz))*24;pz=(gz+.12+.76*rnd(seed,'crack-z',gx,gz))*24
                    distance=np.hypot(XX-px,ZZ-pz);second=np.minimum(second,np.maximum(near,distance));near=np.minimum(near,distance)
            crust=core&((second-near)>.7);cells={}
            for z,x in seen:
                top=94 if crust[z,x] else 48 if inner[z,x] else 5
                cells[int(XX[z,x]),int(ZZ[z,x])]=(int(target[z,x]),3,top)
            centre=[int(XX[iz,ix]),int(ZZ[iz,ix])]
            add('playa',cells,centre=centre,floor=floor,spill=spill,level=level,area=len(cells),saltColumns=int(crust.sum()),maximumRaise=int((target-H).max()),maximumCut=0)
        # Priority drainage assigns every coarse cell a lower/equal route to
        # an existing basin. Parent order resolves flats without routing cycles.
        shape=h.shape;dist=np.full(shape,np.inf);parents={};heap=[];seen=set()
        for z in range(shape[0]):
            for x in [0,shape[1]-1]:dist[z,x]=h[z,x];heapq.heappush(heap,(int(h[z,x]),z,x));seen.add((z,x))
        for x in range(1,shape[1]-1):
            for z in [0,shape[0]-1]:dist[z,x]=h[z,x];heapq.heappush(heap,(int(h[z,x]),z,x));seen.add((z,x))
        while heap:
            value,z,x=heapq.heappop(heap)
            for dz,dx in [(1,0),(-1,0),(0,1),(0,-1),(1,1),(1,-1),(-1,1),(-1,-1)]:
                zz,xx=z+dz,x+dx;k=zz,xx
                if 0<=zz<shape[0] and 0<=xx<shape[1] and k not in seen:
                    seen.add(k);v=max(value,int(h[k]));dist[k]=v;parents[k]=(z,x);heapq.heappush(heap,(v,zz,xx))
        accumulation=np.ones(shape,dtype=int)
        # Reverse flood discovery follows children before their parents.
        for k in reversed(list(parents)):
            accumulation[parents[k]]+=accumulation[k]
        starts=[k for k in parents if ok[k] and h[k]>=90 and accumulation[k]>=3]
        starts.sort(key=lambda k:(-h[k],-accumulation[k],rnd(seed,'start',*k)))
        checked=0
        for start in starts:
            if sum(v['kind']=='wash' for v in features)>=3 or checked>=50:break
            if any(math.dist([X[start],Z[start]],v['centre'])<200 for v in features):continue
            route=[];k=start;ending=None
            for _ in range(80):
                route.append(k)
                p=parents.get(k)
                if p is None:break
                if not ok[p]:
                    if a[p][3]>=h[p]:ending='water-margin'
                    break
                # No trench through divides: uphill steps over two blocks
                # end at the natural dry sink, never a manufactured channel.
                if h[p]>min(h[t] for t in route)+2:ending='dry-basin';break
                k=p
                if len(route)>14 and h[k]<90 and accumulation[k]>12:ending='infiltration-fan';route.append(k);break
            path=[[int(X[k]),int(Z[k])] for k in route]
            length=sum(math.dist(u,v) for u,v in zip(path,path[1:]))
            if not ending or not 160<=length<=900 or h[start]-h[route[-1]]<12:continue
            checked+=1
            lo=np.min(path,axis=0)-28;hi=np.max(path,axis=0)+28;n2=int(max(hi-lo)+1)
            if n2>720 or lo[0]<-3920 or lo[1]<-3920 or lo[0]+n2>3920 or lo[1]+n2>3920:continue
            b=scan(int(lo[0]),int(lo[1]),n2,1,f'wash-{checked}');H=b[:,:,2].astype(int);XX=b[:,:,0];ZZ=b[:,:,1];W=weights(j,XX,ZZ)
            centreline=[np.array(p,float) for p in path]
            for _ in range(2):
                centreline=[centreline[0]]+[v for u,vv in zip(centreline,centreline[1:]) for v in (.75*u+.25*vv,.25*u+.75*vv)]+[centreline[-1]]
            centreline=[p.tolist() for p in centreline]
            distance,station=tube(XX,ZZ,centreline);width=(4+7*station)*(1+.09*np.sin(station*11+rnd(seed,'width',*start)*6.28))
            body=1-smooth(distance/width);ends=smooth(station/.12)*smooth((1-station)/.24)
            change=np.floor((1.1+.9*station)*body*ends+.5).astype(int)
            affected=distance<width
            if not np.all(valid(b,W)[dilate(affected,12)]):rejected['wash-water-or-special-buffer']+=1;continue
            # Fine centreline must already run downhill within the shallow
            # cut budget; coarse routing cannot excuse cutting across dunes.
            line=[]
            for u,v in zip(centreline,centreline[1:]):
                for t in np.linspace(0,1,math.ceil(max(abs(v[0]-u[0]),abs(v[1]-u[1])))+1):
                    pt=(round(u[0]+(v[0]-u[0])*t),round(u[1]+(v[1]-u[1])*t))
                    if not line or pt!=line[-1]:line.append(pt)
            heights=[int(H[z-int(lo[1]),x-int(lo[0])]) for x,z in line]
            if max(v-min(heights[:i+1]) for i,v in enumerate(heights))>2:rejected['wash-fine-uphill']+=1;continue
            cells={};target=H-change
            for z,x in zip(*np.where(change>0)):
                cells[int(XX[z,x]),int(ZZ[z,x])]=(int(target[z,x]),1,5)
            if any(k in used for k in cells):continue
            mid=path[len(path)//2]
            if not add('wash',cells,centre=mid,path=centreline,length=round(length,1),sourceHeight=int(h[start]),endHeight=int(h[route[-1]]),receiver=ending,maximumCut=int(change.max()),maximumRaise=0):continue
            # A low, sand-fed fan spreads at the foot of the same drainage.
            # Fit the cone to actual downstream grades; no gravel biome/rocks.
            for i in range(3,len(route)-4):
                upstream=(h[route[i-3]]-h[route[i]])/max(1,math.dist(path[i-3],path[i]))
                downstream=(h[route[i]]-h[route[min(len(route)-1,i+4)]])/max(1,math.dist(path[i],path[min(len(route)-1,i+4)]))
                if upstream<.20 or not 0<=downstream<min(.18,upstream*.7):continue
                apex=np.array(path[i],float);direction=np.array(path[min(len(route)-1,i+4)],float)-apex;direction/=max(1,np.linalg.norm(direction))
                u=(XX-apex[0])*direction[0]+(ZZ-apex[1])*direction[1];v=-(XX-apex[0])*direction[1]+(ZZ-apex[1])*direction[0]
                lengthFan=min(120.,max(64.,math.dist(path[i],path[-1])))
                lateral=np.abs(v)/(5+.42*np.maximum(u,0));foot=1-smooth((u/lengthFan-.60)/.40)
                amount=2.4*smooth(u/24)*foot*(1-smooth((lateral-.20)/.8))*smooth((distance-width-2)/12)
                maskFan=(u>0)&(u<lengthFan)&(lateral<1)&(distance>width+2)
                deposit=np.where(maskFan,np.floor(amount+.5).astype(int),0)
                fanMask=deposit>0
                if fanMask.sum()<250 or not np.all(valid(b,W)[dilate(fanMask,16)]):continue
                # Never raise a dry channel, ridge or unrelated dune to fit a cone.
                apexY=float(h[route[i]]);grade=max(.025,downstream)
                relative=H-(apexY-grade*u)
                fit=smooth((relative+7)/3)*(1-smooth((relative+1)/3))
                deposit=np.where(maskFan,np.floor(amount*fit+.5).astype(int),0);fanMask=deposit>0
                if fanMask.sum()<250:continue
                acceptable,reason=fan_visibility(H,deposit,u,v,lengthFan,apexY,grade)
                if not acceptable:rejected[reason]+=1;continue
                fan={}
                for z,x in zip(*np.where(fanMask)):
                    key=int(XX[z,x]),int(ZZ[z,x])
                    if key not in used:fan[key]=(int(H[z,x]+deposit[z,x]),2,5)
                centre=list(map(int,np.array(path[i])+direction*lengthFan*.4))
                if add('fan',fan,centre=centre,apex=path[i],channel=len(features)-1,length=round(lengthFan,1),upstreamSlope=float(upstream),downstreamSlope=float(downstream),maximumRaise=int(deposit.max()),maximumCut=0):break
        print(json.dumps(dict(coarseShape=list(h.shape),closedCandidates=len(groups),routesTried=checked,rejected=dict(rejected))),flush=True)
    # Check exact final surfaces, not just the compact height grid. This
    # protects wetlands, existing recesses and height-dependent biome masks
    # on other seeds before accepting anything into a runnable package.
    if features:
        points=sorted(used)
        query=''.join(json.dumps([x+32768,z+32768])+'\n' for x,z in points)
        result=subprocess.run([str(E),str(base)],input=query,text=True,capture_output=True,check=True)
        sampled=dict(zip(points,[json.loads(line) for line in result.stdout.splitlines()]))
        kept=[];indices={}
        for i,f in enumerate(features):
            bad=False
            adjusted={}
            for z,x0,x1,h,ty,top in f['rows']:
                for x in range(x0,x1+1):
                    s=sampled[x,z]
                    if s['water']>=s['height'] or s['desert']<=.4 or not -2<=h-s['height']<=2 or s['cliffLow']!=-32768 or s['riverCutLow']!=-32768 or any(s[k]>.001 for k in ['lake','river','fluvialType','stone','volcanic','giant','scarp','wetland']):bad=True;break
                    # An ephemeral mountain wash exposes the local substrate,
                    # not a fixed strip of sand climbing into the foothills.
                    if ty==1 and j['config']['deserts'].get('foothillSandCover')==1:
                        top=s['surfaceMaterial'];top=3 if top==4 else top
                        if top not in (2,3,5,6,33):bad=True;break
                    adjusted[x,z]=(h,ty,top)
                if bad:break
            if f['kind']=='fan' and f['channel'] not in indices:bad=True
            if bad:rejected['exact-final-surface']+=1;continue
            f['rows']=encode(adjusted)
            if f['kind']=='fan':f['channel']=indices[f['channel']]
            indices[i]=len(kept);kept.append(f)
        features=kept;used={(x,z) for f in features for z,x0,x1,*_ in f['rows'] for x in range(x0,x1+1)}
    package=dict(algorithm='arid-dry-landforms-v1',seed=seed,sourceSha256=identity(j),features=features)
    j['aridLandforms']=package;source.write_text(json.dumps(j,separators=(',',':')),'utf-8')
    report=dict(seed=seed,features=[{k:v for k,v in f.items() if k!='rows'} for f in features],columns=len(used),spans=sum(len(f['rows']) for f in features),rejected=dict(rejected),runtimeTerrainQueriesAdded=0)
    (out/'arid-plan.json').write_text(json.dumps(report,indent=2),'utf-8');print(json.dumps(report),flush=True)
    return package

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();bake(a.source,a.output)
