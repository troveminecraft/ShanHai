"""Finite clay terraces fitted to lush chamber floors; native runtime places runs."""
import json,random
from pathlib import Path
from world_bounds import HALF
import numpy as np
from runtime_palette import sizes as palette_sizes
DRY=[0,28,29,30,31,37,67,68,120,121,122,123,124,125]
SOLID=[2,6,48,80,94,95,96,97,98,99]+list(range(100,116))
def fit_pools(a,x0,z0,seed,max_pools=6):
 # Segment actual cave-floor contours, rather than scatter geometric bowls.
 # All eligibility comes from immutable pre-terrace blocks. A rock column or
 # an existing watercourse cuts the mask instead of being overwritten.
 from collections import deque
 sz,sx,_=a.shape
 clear=np.isin(a,DRY);solid=np.isin(a,SOLID)
 valid=solid[:,:,:-6].copy()
 for dy in range(1,7):valid &= clear[:,:,dy:dy+378]
 valid[:,:,:20]=False;valid[:,:,164:]=False
 valid[:4]=False;valid[-4:]=False;valid[:,:4]=False;valid[:,-4:]=False
 nodes={tuple(map(int,v)) for v in np.argwhere(valid)}
 groups=[]
 while nodes:
  root=min(nodes);nodes.remove(root);queue=[root]
  for z,x,k in queue:
   for dz,dx in [(1,0),(-1,0),(0,1),(0,-1)]:
    for dk in [-2,-1,0,1,2]:
     t=(z+dz,x+dx,k+dk)
     if t in nodes:nodes.remove(t);queue.append(t)
  if len(queue)>=180 and sum(a[z,x,k]==80 for z,x,k in queue)>=30:groups.append(queue)
 groups.sort(key=lambda g:(-len(g),min(g)))
 terraces=[]
 for group in groups[:3]:
  # A single floor graph can fold back into a vertically stacked room; keep
  # those out instead of blending floors from different elevations.
  # A winding passage below a hall can share a few X/Z columns with it.
  # Exclude those ambiguous columns instead of rejecting the entire chamber.
  from collections import Counter
  multiplicity=Counter((z,x) for z,x,k in group)
  floor={(z,x):k for z,x,k in group if multiplicity[z,x]==1}
  if len(floor)<180:continue
  # Contour bands span the actual floor laterally. Never restrict them to
  # a drainage path: that creates a necklace of pools instead of terraces.
  # Reject voxel-scale bumps before contouring. This does not smooth the
  # cave itself: it only chooses a terrace level, bounded by local floor data.
  bandlevel={p:int(np.median([floor[q] for dz in (-1,0,1) for dx in (-1,0,1)
   if (q:=(p[0]+dz,p[1]+dx)) in floor]))//3*3+1 for p in floor}
  levels=set(bandlevel.values())
  if len(levels)<3:continue
  remaining=set(floor);patches=[]
  while remaining:
   root=min(remaining);remaining.remove(root);level=bandlevel[root];queue=[root]
   for z,x in queue:
    for dz,dx in [(1,0),(-1,0),(0,1),(0,-1)]:
     t=z+dz,x+dx
     if t in remaining and bandlevel[t]==level:remaining.remove(t);queue.append(t)
   region=set(queue)
   # Do not turn the finite sampling window into a rectangular dam. Only
   # complete contours qualify; incomplete bands remain original terrain.
   if any(z in (4,sz-5) or x in (4,sx-5) for z,x in region):continue
   # One block rim, shaped by the contour rather than an ellipse. Erode at
   # all external contacts including cliff edges, pillars and water buffers.
   wet={p for p in region if all((p[0]+dz,p[1]+dx) in region and
    all((p[0]+dz+ez,p[1]+dx+ex) in floor for ez,ex in [(1,0),(-1,0),(0,1),(0,-1)])
    for dz,dx in [(1,0),(-1,0),(0,1),(0,-1)])}
   if len(wet)>=30 and max(max(p[d] for p in wet)-min(p[d] for p in wet)+1 for d in (0,1))>=18:patches.append((level,region,wet))
  if len({v[0] for v in patches})<3:continue
  # Retain only terraces next to another accepted height band: no isolated
  # pocket ponds elsewhere in the room.
  owner={p:i for i,(_,region,_) in enumerate(patches) for p in region}
  neighbours={i:set() for i in range(len(patches))}
  for p,i in owner.items():
   for dz,dx in [(1,0),(-1,0),(0,1),(0,-1)]:
    j=owner.get((p[0]+dz,p[1]+dx))
    if j is not None and j!=i and abs(patches[i][0]-patches[j][0])==3 and abs(floor[p]-floor[(p[0]+dz,p[1]+dx)])<=2:neighbours[i].add(j)
  connected={i for i,ns in neighbours.items() if ns}
  if len({patches[i][0] for i in connected})<3:continue
  for i in sorted(connected):
   level,region,wet=patches[i];placed=[]
   for z,x in sorted(region):
    k=floor[z,x];col=a[z,x]
    # Outer edge keeps the original ground elevation; this feathers the
    # terrace back into the cave without creating a raised perimeter wall.
    border=any((z+dz,x+dx) not in floor for dz,dx in [(1,0),(-1,0),(0,1),(0,-1)])
    if border:continue
    # Solid rims share the water block layer; an extra block is not needed
    # for containment and creates the unwanted raised lip.
    top=level
    bottom=min(k-1,level-2)
    col[bottom:top+1]=48
    col[top+1:max(k+3,top+5)]=0
    if (z,x) in wet:col[level]=8
    else:
     pick=((x+x0)*73856093 ^ (z+z0)*19349663 ^ seed)&0xffff
     if pick%23==0:
      d=pick%4
      if pick%2:col[top+1]=135+d;col[top+2]=139+d
      else:col[top+1:top+3]=127+d;col[top+3]=131+d
    placed.append((z,x,top))
   # Existing terrain at external boundaries is never forced into a dam.
   # Erosion above excludes every unmodified boundary from wet contacts.
   if placed:
    placed_set={(v[0],v[1]) for v in placed}
    z,x,_=min(placed,key=lambda v:abs(v[0]-sz//2)+abs(v[1]-sx//2))
    terraces.append(dict(center=[x+x0,level-64,z+z0],level=level-64,
      contour=True,neighbourLevels=sorted({patches[j][0]-64 for j in neighbours[i]}),
      cells=[[xx+x0,zz+z0,tt-64] for zz,xx,tt in placed],
      waterCells=[[xx+x0,level-64,zz+z0] for zz,xx in sorted(wet) if (zz,xx) in placed_set]))
 return terraces

def runs(a,b,x0,z0):
 result=[]
 for z,x in np.argwhere((a!=b).any(axis=2)):
  changed=np.flatnonzero(a[z,x]!=b[z,x]);i=0
  while i<len(changed):
   k=int(changed[i]);end=k;v=int(b[z,x,k]);i+=1
   while i<len(changed) and int(changed[i])==end+1 and int(b[z,x,changed[i]])==v:end+=1;i+=1
   result.append([int(x)+x0,int(z)+z0,k-64,end-64,v])
 return result

def bake(source,out,groups,grotto):
 from bake_stone_grotto import P
 from native_chunk_io import run,decode
 out=Path(out);out.mkdir(parents=True,exist_ok=True)
 j=json.loads(Path(source).read_text('utf-8'));seed=j['config']['seed'];E=P/'build/windows/x64/release/native-worldgen-check.exe'
 nodes=[n for g in groups for n in g['nodes'] if n['hall'] and -35<n['y']<100]
 nodes.sort(key=lambda n:(int(n['x'])*73856093 ^ int(n['z'])*19349663 ^ seed)&0xffffffff)
 features=[]
 climates=run(E,source,[[int(n['x'])+32768,int(n['z'])+32768] for n in nodes])
 candidates=[n for n,h in zip(nodes,climates) if h.get('glacier',0)<=.3 and h['H']>.43 and h['desert']<.1]
 for n in candidates[:64]:
  x0=(int(n['x'])//16-4)*16;z0=(int(n['z'])//16-4)*16
  if min(x0,z0)<-HALF or max(x0+144,z0+144)>HALF:continue
  if any(not(x0+143<f['bounds'][0] or x0>f['bounds'][2] or z0+143<f['bounds'][1] or z0>f['bounds'][3]) for f in grotto+features):continue
  coords=[(x,z) for z in range(z0//16,z0//16+9) for x in range(x0//16,x0//16+9)]
  run(E,source,[dict(cx=x,cz=z,output=str((out/f'{x}-{z}.bin').resolve())) for x,z in coords])
  a=np.zeros((144,144,384),np.uint16)
  for x,z in coords:a[z*16-z0:z*16-z0+16,x*16-x0:x*16-x0+16]=decode(out/f'{x}-{z}.bin',*palette_sizes())
  if np.count_nonzero(a[:,:,20:160]==80)<100:continue
  b=a.copy();ponds=fit_pools(b,x0,z0,seed)
  if not ponds:continue
  f=dict(bounds=[x0,z0,x0+143,z0+143],clayPools=ponds,runs=runs(a,b,x0,z0));features.append(f)
  np.savez_compressed(out/f'volume-{len(features)}.npz',before=a,after=b,x0=x0,z0=z0)
  if len(features)>=8:break
 (out/'plan.json').write_text(json.dumps([{k:v for k,v in f.items() if k!='runs'} for f in features],indent=2),'utf-8')
 return features
