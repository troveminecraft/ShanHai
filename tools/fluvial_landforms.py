"""Finite seed-time selection of bars and abandoned meanders in real river valleys.

Only small accepted footprints are frozen; no world chunks or per-tick erosion.
The main drainage graph, receiver levels and centreline remain unchanged.
"""
from pathlib import Path
from world_bounds import HALF
import argparse, collections, hashlib, json, math, subprocess
import numpy as np

P=Path(__file__).resolve().parents[1]
E=P/'build/windows/x64/release/native-worldgen-check.exe'
def digest(j):return hashlib.sha256(json.dumps(j,sort_keys=True,separators=(',',':')).encode()).hexdigest()
def smooth(v):
    t=np.clip(v,0,1);return t*t*(3-2*t)
def identity(j):
    r={k:v for k,v in j['rivers'].items() if k!='landforms'}
    return digest(dict(config=j['config'],base=j['naturalLakeSource'],lakes=j['naturalLakes'],rivers=r))
def rnd(seed,*keys):return int(digest([seed,*keys])[:12],16)/16**12
def sample(source,pts):
    q=''.join(json.dumps([int(x)+32768,int(z)+32768])+'\n' for x,z in pts)
    p=subprocess.run([str(E),str(source)],input=q,text=True,capture_output=True,check=True)
    return [json.loads(s) for s in p.stdout.splitlines()]
def bounds(pts,halo):
    a=np.asarray(pts);lo=np.floor(a.min(axis=0)-halo).astype(int);hi=np.ceil(a.max(axis=0)+halo).astype(int)
    return int(lo[0]),int(lo[1]),int(hi[0]-lo[0]+1),int(hi[1]-lo[1]+1)
def grid(b):
    x,z,w,h=b;zz,xx=np.mgrid[z:z+h,x:x+w];return xx,zz
def tube(xx,zz,path):
    distance=np.full(xx.shape,np.inf);station=np.zeros(xx.shape)
    total=sum(math.dist(a,b) for a,b in zip(path,path[1:]));start=0
    for a,b in zip(path,path[1:]):
        dx,dz=b[0]-a[0],b[1]-a[1];length=math.hypot(dx,dz)
        t=np.clip(((xx-a[0])*dx+(zz-a[1])*dz)/max(.001,length*length),0,1)
        d=np.hypot(xx-a[0]-t*dx,zz-a[1]-t*dz);take=d<distance
        station[take]=(start+t[take]*length)/max(1,total);distance=np.minimum(distance,d);start+=length
    return distance,station
def encode(cells):
    rows=[]
    for (x,z),v in sorted(cells.items(),key=lambda kv:(kv[0][1],kv[0][0])):
        if rows and rows[-1][0]==z and rows[-1][2]+1==x and rows[-1][3:]==list(v):rows[-1][2]=x
        else:rows.append([z,x,x,*v])
    return rows

def runtime_compatible(cells):
    # Match the native immutable-span contract at candidate selection time.
    # Reject a whole feature rather than clamp its toe into an artificial ledge.
    return all(-HALF<=x<HALF and -HALF<=z<HALF and 28<=h<=110
               and (w==-32768 or h<w<=h+16) and 1<=kind<=3
               and top in (3,4,5,33,42,48,85)
               for (x,z),(h,w,kind,top) in cells.items())


def bake(j,out):
    out.mkdir(parents=True,exist_ok=True)
    j['rivers'].pop('landforms',None)
    source=out/'landform-base-input.json';source.write_text(json.dumps(j,separators=(',',':')),'utf-8')
    seed=j['config']['seed'];segments={tuple(s[:4]):s for s in j['rivers']['segments']}
    ports=[r['join'] for r in j['rivers']['routes']]+[r['source'] for r in j['rivers']['routes']]
    candidates=[]
    for mi,m in enumerate(j['rivers'].get('mainstems',[])):
        rr=m['reaches']
        for i in range(4,len(rr)-5):
            r=rr[i];s=segments.get(tuple(r['a']+r['b']))
            if not s or not 35<=s[5]<=90 or s[4]-s[5]>.3 or min(s[6:])<12:continue
            p=np.array(r['a'],dtype=float);t=np.array(rr[i+2]['b'],dtype=float)-np.array(rr[i-2]['a'],dtype=float);t/=max(1,np.linalg.norm(t))
            if min(math.dist(p,q) for q in ports)<110:continue
            if abs(r['bend'])>.3:continue
            for side in (-1,1):
                candidates.append(dict(main=mi,index=i,p=p,t=t,n=np.array([-t[1],t[0]])*side,
                                       water=int(s[5]),radius=min(s[6:]),side=side,
                                       rank=rnd(seed,mi,i,side)))
    candidates.sort(key=lambda c:c['rank'])
    accepted=[];occupied=[];rejected=collections.Counter()
    def available(p,spacing):return all(math.dist(p,q)>spacing for q in occupied)
    def collect(path,halo):
        b=bounds(path,halo)
        if b[0]<-(HALF-144) or b[1]<-(HALF-144) or b[0]+b[2]>(HALF-144) or b[1]+b[3]>(HALF-144):return None
        xx,zz=grid(b);rows=sample(source,zip(xx.flat,zz.flat))
        a={k:np.array([r[k] for r in rows]).reshape(xx.shape) for k in ['height','water','river','lake','stone','volcanic','barren','giant','scarp','wetland','riverBankWater']}
        return b,xx,zz,a
    def allowed(a,mask):
        return not any(np.any(a[k][mask]>.01) for k in ('lake','stone','volcanic','barren','giant','scarp','wetland'))
    # At most two elongated deposits per long mainstem, displaced from its
    # thalweg. Both side passages must already exist as live water.
    for c in candidates:
        if sum(f['kind']=='island' for f in accepted)>=max(1,min(4,len(j['rivers'].get('mainstems',[]))*2)):break
        p,t,n,level,r=c['p'],c['t'],c['n'],c['water'],c['radius']
        if not available(p,420):continue
        half=18+8*rnd(seed,'bar-length',c['main'],c['index']);wide=2.8+min(1.7,(r-12)*.25)
        centre=p+n*(wide+4.5);path=[centre-t*half,centre+t*half]
        packed=collect(path,wide+5)
        if packed is None:continue
        b,xx,zz,a=packed;along=(xx-centre[0])*t[0]+(zz-centre[1])*t[1];across=(xx-centre[0])*n[0]+(zz-centre[1])*n[1]
        # Broad asymmetry, not per-block random bumps or repeated ellipses.
        shift=.8*np.sin(along/13+c['rank']*6.28)
        q=np.sqrt((along/half)**2+((across-shift)/wide)**2)
        toe=np.sqrt((along/(half+5))**2+((across-shift)/(wide+3))**2)
        support=toe<1.12
        if not allowed(a,support) or not np.all((a['river'][support]>0)&(a['water'][support]==level)&(a['height'][support]<level)):
            rejected['bar-needs-wide-level-water']+=1;continue
        crest=level+1-np.maximum(0,q-.42)*2.1
        blend=1-smooth((toe-.58)/.42)
        h=np.maximum(a['height'],np.floor(a['height']+blend*(crest-a['height'])+.5).astype(int))
        changed=(h>a['height'])&(toe<1);land=changed&(h>=level)
        if np.count_nonzero(land)<20:rejected['bar-too-small']+=1;continue
        # No island touches the graph's protected central navigation lane.
        clearance=np.full(xx.shape,np.inf)
        for rs in j['rivers']['segments']:
            if math.dist(p,rs[:2])>120:continue
            d,_=tube(xx,zz,[rs[:2],rs[2:4]]);clearance=np.minimum(clearance,d)
        if np.any(changed&(clearance<1.5)) or np.any(land&(clearance<3.5)):
            rejected['bar-thalweg']+=1;continue
        # A two-block water corridor around the emerged land prevents an
        # incidental point bar being labelled as an island.
        dil=land.copy()
        for _ in range(2):dil|=np.roll(dil,1,0)|np.roll(dil,-1,0)|np.roll(dil,1,1)|np.roll(dil,-1,1)
        if np.any((dil&~land)&(h>=level)):rejected['bar-side-passage']+=1;continue
        cells={}
        for iz,ix in zip(*np.where(changed)):
            v=int(h[iz,ix]);top=4 if v>level and q[iz,ix]<.6 else 5
            cells[int(xx[iz,ix]),int(zz[iz,ix])]=(v,level if v<level else -32768,1,top)
        if not runtime_compatible(cells):
            rejected['bar-runtime-envelope']+=1;continue
        point=tuple(map(int,np.unravel_index(np.argmax(np.where(land,h,-999)),h.shape)))
        iz,ix=point
        accepted.append(dict(kind='island',mainstem=c['main'],anchor=list(map(float,p)),level=level,bounds=b,
                             centre=[int(xx[iz,ix]),int(zz[iz,ix])],length=round(half*2,1),landArea=int(land.sum()),
                             rows=encode(cells),sidePassages=2));occupied.append(centre)
    # A former meander is anchored to two stations of the same current river,
    # with sediment plugs at its ends. Only broad, low floodplains qualify.
    for kind in ('oxbow','abandoned'):
        found=0
        for c in candidates:
            if found>=min(2,len(j['rivers'].get('mainstems',[]))):break
            p,t,n,level,r=c['p'],c['t'],c['n'],c['water'],c['radius']
            if not available(p,140):continue
            half=37+16*rnd(seed,'old-length',c['main'],c['index']);bulge=26+12*rnd(seed,'old-bulge',c['main'],c['index'])
            bank=r+9;ts=np.linspace(0,1,41)
            path=[p+t*((v-.5)*half*2)+n*(bank+bulge*math.sin(math.pi*v)**.8) for v in ts]
            width=5.5+1.5*rnd(seed,'old-width',c['main'],c['index'])
            # Cheap coarse screening prevents expansive full-res sampling of
            # mountains and existing water/special biomes.
            probes=[q+n*d for q in path[::4] for d in (-8,0,8)]
            rows=sample(source,[(round(q[0]),round(q[1])) for q in probes])
            if any(any(v[k]>.01 for k in ('lake','stone','volcanic','barren','giant','scarp','wetland')) or v['river'] or v['water']>=v['height'] or v['height']<level or v['height']>level+7 for v in rows):
                rejected[kind+'-not-floodplain']+=1;continue
            packed=collect(path,width+7)
            if packed is None:continue
            b,xx,zz,a=packed;dist,station=tube(xx,zz,path)
            localWidth=width*(1+.12*np.sin(station*9+c['rank']*6.28))
            body=1-smooth(dist/localWidth)
            ends=smooth(station/.16)*smooth((1-station)/.16)
            mask=dist<localWidth+2
            if not allowed(a,mask) or np.any((a['river'][mask]>0)|(a['water'][mask]>=a['height'][mask])):
                rejected[kind+'-touches-water']+=1;continue
            # Never build a levee or flood lower neighbouring terrain.
            # The residual lake is on the river's floodplain water datum.
            if np.min(a['height'][mask])<level or np.max(a['height'][mask])-level>7:
                rejected[kind+'-bank-height']+=1;continue
            if kind=='oxbow':
                depth=2.2+.45*np.sin(station*5+c['rank']*6.28)
                target=level-depth
                h=np.floor(a['height']+body*ends*(target-a['height'])+.5).astype(int)
                if np.max(a['height'][mask]-h[mask])>5:rejected['oxbow-cut-budget']+=1;continue
                wet=(h<level)&mask
                if np.count_nonzero(wet)<100:rejected['oxbow-too-small']+=1;continue
            else:
                h=a['height']-np.floor(2.1*body*ends+.5).astype(int)
                h=np.where(mask,np.maximum(h,level),a['height']);wet=np.zeros(h.shape,bool)
            changed=(h!=a['height'])&mask;cells={}
            assert not np.any(h[changed]>a['height'][changed]),'Old channel must never raise terrain'
            for iz,ix in zip(*np.where(changed)):
                v=int(h[iz,ix]);water=level if wet[iz,ix] else -32768
                # Lakes retain the user's sand/gravel bottom convention;
                # an abandoned dry channel uses the local soil family.
                top=(5 if rnd(seed,'oxbow-bed',int(xx[iz,ix]),int(zz[iz,ix]))<.5 else 42) if wet[iz,ix] else 33 if body[iz,ix]>.25 else 4
                cells[int(xx[iz,ix]),int(zz[iz,ix])]=(v,water,2 if kind=='oxbow' else 3,top)
            if not runtime_compatible(cells):
                rejected[kind+'-runtime-envelope']+=1;continue
            for axis in (0,1):
                other=np.roll(h,1,axis);ow=np.roll(wet,1,axis)
                assert not np.any(wet&~ow&(other<level)), 'Uncontained remnant lake'
            middle=path[len(path)//2];iz,ix=round(middle[1])-b[1],round(middle[0])-b[0]
            accepted.append(dict(kind=kind,mainstem=c['main'],anchor=list(map(float,p)),level=level,bounds=b,
                                 centre=[int(xx[iz,ix]),int(zz[iz,ix])],formerPath=[list(map(float,v)) for v in path],
                                 wetArea=int(wet.sum()),maximumCut=int((a['height']-h).max()),rows=encode(cells)))
            occupied.append(p);found+=1
    used=set()
    for f in accepted:
        for z,x0,x1,*_ in f['rows']:
            for x in range(x0,x1+1):
                assert (x,z) not in used,'Feature overlap';used.add((x,z))
    package=dict(algorithm='floodplain-landforms-v1',seed=seed,sourceSha256=identity(j),features=accepted)
    j['rivers']['landforms']=package
    report=dict(seed=seed,candidates=len(candidates),features=[{k:v for k,v in f.items() if k not in ('lavaFlows','rows','formerPath')} for f in accepted],
                changedColumns=len(used),rowSpans=sum(len(f['rows']) for f in accepted),rejected=dict(rejected),runtimeTerrainQueriesAdded=0)
    (out/'landform-plan.json').write_text(json.dumps(report,indent=2),'utf-8')
    print(json.dumps(report),flush=True)
    return package


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    j=json.loads(a.source.read_text('utf-8'));bake(j,a.output)
    j['riverSource']=digest(dict(base=j['naturalLakeSource'],lakes=j['naturalLakes'],rivers=j['rivers']))
    if 'overhangCliffs' in j:j['overhangCliffs']['sourceSha256']=digest({k:v for k,v in j.items() if k not in ('lavaFlows','iceRegions','version','overhangCliffs','cavernPlan','stoneForestGrotto')})
    a.source.write_text(json.dumps(j,separators=(',',':')),'utf-8')
