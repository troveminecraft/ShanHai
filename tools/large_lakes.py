"""Recognize connected large natural basins; no artificial lake excavation."""
from pathlib import Path
from world_bounds import HALF, SIDE, CHUNKS
from collections import deque
import json
import subprocess
import numpy as np

MIN_AREA = 16384
MAX_AREA = 110000
MAX_LAKES = 6
MAX_DEPTH = 80
LOWLAND_MAX_AREA = 90000
UPLAND_MIN_LEVEL = 120
MAX_SPAN = 480

def fit_size(heights, start, proposed_level):
    """Lower retained water, not terrain, until the connected basin fits.

    Evaluate the lowland and upland elevation ranges independently: changing
    the area budget at Y=120 must not break the binary search's monotonicity.
    """
    floor=int(heights[start]);best=None
    for low,high,cap in ((max(34,floor+3),min(proposed_level,UPLAND_MIN_LEVEL-1),LOWLAND_MAX_AREA),
                         (max(UPLAND_MIN_LEVEL,floor+3),proposed_level,MAX_AREA)):
        while low<=high:
            level=(low+high)//2;wet=component(heights,start,level)
            zz,xx=np.where(wet);area=int(wet.sum())
            span=[int(xx.max()-xx.min()+1),int(zz.max()-zz.min()+1)]
            if area<=cap and max(span)<=MAX_SPAN:
                if area>=MIN_AREA:best=(level,wet,span)
                low=level+1
            else:high=level-1
    return best

def component(height, start, level):
    h,w=height.shape
    mask=np.zeros((h,w),bool)
    if height[start]>=level:return mask
    q=deque([start]);mask[start]=True
    while q:
        z,x=q.popleft()
        for dz,dx in ((1,0),(-1,0),(0,1),(0,-1)):
            nz,nx=z+dz,x+dx
            if 0<=nz<h and 0<=nx<w and not mask[nz,nx] and height[nz,nx]<level:
                mask[nz,nx]=True;q.append((nz,nx))
    return mask

def ring(mask, distance=1):
    expanded=mask.copy()
    for _ in range(distance):
        nxt=expanded.copy()
        nxt[1:]|=expanded[:-1];nxt[:-1]|=expanded[1:]
        nxt[:,1:]|=expanded[:,:-1];nxt[:,:-1]|=expanded[:,1:]
        expanded=nxt
    return expanded&~mask

def encode(wet, bank, x0, z0):
    rows=[]
    for mask,kind in ((wet,1),(bank,0)):
        for z in range(mask.shape[0]):
            changes=np.diff(np.pad(mask[z].astype(np.int8),(1,1)))
            starts=np.flatnonzero(changes==1);ends=np.flatnonzero(changes==-1)-1
            rows.extend([z+z0,int(x)+x0,int(end)+x0,kind] for x,end in zip(starts,ends))
    return rows

def outlet_starts(wet, heights, x0, z0, level):
    # Prefer shallow natural saddles over the shoreline nearest a downstream
    # river. The nearest bank can be a steep ridge on a large irregular lake.
    h,w=wet.shape;options=[]
    for z in range((-z0)%4,h,4):
        for x in range((-x0)%4,w,4):
            if not wet[z,x]:continue
            outside=[int(heights[z+dz,x+dx]) for dz,dx in ((4,0),(-4,0),(0,4),(0,-4))
                     if 0<=z+dz<h and 0<=x+dx<w and not wet[z+dz,x+dx]]
            if outside and min(outside)<=level+4:options.append((min(outside),x+x0,z+z0))
    selected=[]
    for height,x,z in sorted(options):
        if all((x-a[0])**2+(z-a[1])**2>=32**2 for a in selected):selected.append([x,z,height])
        if len(selected)>=32:break
    return selected

def candidates(coarse, fill):
    h=coarse[:,2].astype(int).reshape(CHUNKS,CHUNKS);f=fill(h)
    wet=(f-h>=2)&(h>=34)&(f<=260)
    seen=np.zeros(wet.shape,bool);result=[]
    for zz,xx in zip(*np.where(wet)):
        if seen[zz,xx]:continue
        q=[(int(zz),int(xx))];seen[zz,xx]=True
        for z,x in q:
            for dz,dx in ((1,0),(-1,0),(0,1),(0,-1)):
                nz,nx=z+dz,x+dx
                if 0<=nz<CHUNKS and 0<=nx<CHUNKS and wet[nz,nx] and not seen[nz,nx]:
                    seen[nz,nx]=True;q.append((nz,nx))
        if len(q)*256<MIN_AREA:continue
        floor=min(q,key=lambda p:(h[p],p));zs,xs=zip(*q)
        bounds=[min(xs)*16-HALF-48,min(zs)*16-HALF-48,
                max(xs)*16-HALF+49,max(zs)*16-HALF+49]
        if min(bounds[:2])<-(HALF-96) or max(bounds[2:])>(HALF-96):continue
        if max(bounds[2]-bounds[0],bounds[3]-bounds[1])>1536:continue
        if any(coarse[z*CHUNKS+x,6]>.01 or coarse[z*CHUNKS+x,7]>.01 or coarse[z*CHUNKS+x,9]>0 for z,x in q):continue
        result.append(dict(center=[floor[1]*16-HALF,floor[0]*16-HALF],bounds=bounds,
                           approximateArea=len(q)*256,spill=int(f[floor])))
    # Keep offline seed baking bounded when the playable square is enlarged;
    # the strongest retained basins are enough to preserve the feature.
    return sorted(result,key=lambda c:(-c['approximateArea'],c['center']))[:14]

def bake_large(exe, source, coarse, out, fill):
    lakes=[];details=[];rejected=[];occupied={}
    for i,c in enumerate(candidates(coarse,fill)):
        if len(lakes)>=MAX_LAKES:break
        x0,z0,x1,z1=c['bounds'];w,h=x1-x0,z1-z0
        file=out/f'large-basin-{i}.bin'
        query=dict(basinScan=True,x=x0,z=z0,width=w,height=h,output=str(file))
        subprocess.run([str(exe),str(source)],input=json.dumps(query)+'\n',text=True,capture_output=True,check=True)
        raw=np.fromfile(file,dtype=[('height','<i2'),('flags','u1')]).reshape(h,w)
        heights=raw['height'].astype(int);flags=raw['flags']
        cx,cz=c['center'];sx,sz=cx-x0,cz-z0
        patch=heights[max(0,sz-16):sz+17,max(0,sx-16):sx+17]
        dz,dx=np.unravel_index(patch.argmin(),patch.shape);sx=max(0,sx-16)+int(dx);sz=max(0,sz-16)+int(dz)
        filled=fill(heights);floor=int(heights[sz,sx]);spill=int(filled[sz,sx])
        proposed=min(spill-1,floor+MAX_DEPTH)
        fitted=fit_size(heights,(sz,sx),proposed)
        if fitted is None:
            rejected.append(dict(**c,reason='no medium/upland-sized retained basin'));continue
        level,wet,span=fitted;area=int(wet.sum())
        reason=None
        if not 34<=level<=260 or level-floor<3:reason='no retained freshwater level'
        elif not MIN_AREA<=area<=MAX_AREA:reason='area outside large-lake budget'
        elif wet[0].any() or wet[-1].any() or wet[:,0].any() or wet[:,-1].any():reason='open basin boundary'
        elif np.any(flags[wet]&5):reason='ocean or protected terrain intersects basin'
        edge=ring(wet)
        if reason is None and (np.any(heights[edge]<level) or np.any(flags[edge]&1)):reason='unsafe bank'
        if reason is not None:
            rejected.append(dict(**c,reason=reason,area=area,level=level));continue
        if max(span)<192:continue
        # Include a two-column collar at every shore height, not just flat sand
        # beaches: a steep hillside may contain an alcove below its surface.
        bank=ring(wet,2)
        rows=encode(wet,bank,x0,z0)
        if any(any(a<=x1r and b>=x0r for a,b in occupied.get(z,())) for z,x0r,x1r,_ in rows):continue
        for z,a,b,_ in rows:occupied.setdefault(z,[]).append((a,b))
        lake=dict(level=level,rows=rows,center=[x0+sx,z0+sz],floor=floor,spill=spill,area=area,
                  large=True,sizeClass='medium' if level<UPLAND_MIN_LEVEL else 'upland-large',span=span,basinBounds=[x0,z0,w,h],outletStarts=outlet_starts(wet,heights,x0,z0,level))
        lakes.append(lake)
        details.append(dict(index=i,center=lake['center'],level=level,floor=floor,spill=spill,area=area,
                            span=span,bankColumns=int(bank.sum()),bounds=[x0,z0,w,h],large=True,
                            closedEdges=int(edge.sum()),sealedAlcoveColumns=int(np.count_nonzero(flags[wet|bank]&2))))
        print('large lake', {k:v for k,v in details[-1].items() if k!='bounds'},flush=True)
    (out/'large-lake-rejections.json').write_text(json.dumps(rejected,indent=2),'utf-8')
    return lakes,details,occupied
