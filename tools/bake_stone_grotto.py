"""Fit a bounded stone-forest waterfall/lake structure to actual cave blocks."""
from pathlib import Path
from world_bounds import HALF
import json,sys,hashlib,math
import numpy as np
from runtime_palette import sizes as palette_sizes
P=Path(__file__).resolve().parents[1];from native_chunk_io import run,decode
def identity(j):return hashlib.sha256(json.dumps({k:v for k,v in j.items() if k not in ('lavaFlows','iceRegions','version','stoneForestGrotto')},sort_keys=True,separators=(',',':')).encode()).hexdigest()
def bake(source,out):
 source=Path(source);out=Path(out);out.mkdir(parents=True,exist_ok=True);j=json.loads(source.read_text('utf-8'));E=P/'build/windows/x64/release/native-worldgen-check.exe'
 if not j['config']['cave'].get('stoneForestFalls'):return
 j.pop('stoneForestGrotto',None);source.write_text(json.dumps(j,separators=(',',':')),'utf-8')
 groups=run(E,source,[{'cavernGroups':True}])[0];nodes=[n for g in groups for n in g['nodes'] if n['hall'] and n['y']>-35]
 surfaces=run(E,source,[[int(n['x'])+32768,int(n['z'])+32768] for n in nodes])
 candidates=[n for n,s in zip(nodes,surfaces) if .01<s['stone']<.7 and 32<s['height']<110 and s['volcanic']==0]
 features=[];rejected=[]
 for n in candidates:
  cx,cz=int(n['x'])//16,int(n['z'])//16;x0,z0=(cx-5)*16,(cz-5)*16;size=176
  if min(x0,z0)<-HALF or max(x0+size,z0+size)>HALF:continue
  coords=[(x,z) for z in range(cz-5,cz+6) for x in range(cx-5,cx+6)]
  run(E,source,[dict(cx=x,cz=z,output=str((out/f'base-{x}-{z}.bin').resolve())) for x,z in coords])
  a=np.zeros((size,size,384),np.uint16)
  for x,z in coords:a[(z*16-z0):(z*16-z0+16),(x*16-x0):(x*16-x0+16)]=decode(out/f'base-{x}-{z}.bin',*palette_sizes())
  base_original=a.copy()
  refined=j['config']['cave'].get('habitatRefinement',0)==1
  # Aquatic plants occupy the primary palette: include them when tracing sea
  # connectivity, then reserve the grotto inlet as open water, not kelp stems.
  aquatic=np.isin(a,[49,50]) if refined else np.zeros_like(a,bool)
  # Sea-side sources must connect vertically to the sea datum, not a deep pool.
  sea=np.zeros_like(a,dtype=bool);wet=(a[:,:,95]==8)|aquatic[:,:,95]
  for k in range(95,15,-1):wet=wet&((a[:,:,k]==8)|aquatic[:,:,k]);sea[:,:,k]=wet
  air=np.isin(a,[0,28,29,30,31,37,67,68,116,117,118,119,120,121,122,123,124,125])
  faces=np.zeros_like(sea)
  for dz,dx in [(1,0),(-1,0),(0,1),(0,-1)]:faces|=np.roll(sea,(dz,dx),(0,1))&air
  # Only underground, roofed contact faces belong to the grotto. Never turn
  # an ordinary open coast or the surface waterline into a retaining wall.
  rock=np.isin(a,[2,3,4,5,6,94,95,96,97,98,99])
  roof=np.max(np.where(rock,np.arange(384)[None,None,:],-1),axis=2)
  faces&=np.arange(384)[None,None,:]<roof[:,:,None]-3
  faces[:3]=False;faces[-3:]=False;faces[:,:3]=False;faces[:,-3:]=False
  ports=[]
  for z,x in np.argwhere(faces.any(axis=2)):
   ks=np.flatnonzero(faces[z,x]);top=int(ks[-1])-64
   # Require a real dry cavity under the inlet and enough vertical drop.
   below=np.flatnonzero(~air[z,x,9:ks[-1]+1]);floor=int(below[-1])+9-64 if len(below) else -55
   if top<24 and top-floor>=12 and floor>=-40 and abs(x+x0-n['x'])<75 and abs(z+z0-n['z'])<75:ports.append((top-floor,-math.hypot(x+x0-n['x'],z+z0-n['z']),int(x),int(z),top,floor))
  if not ports:rejected.append([int(n['x']),int(n['z']),'no safe sea inlet']);continue
  _,_,px,pz,top,floor=max(ports)
  # Inward points toward the existing chamber, away from the seawater face.
  dx,dz=n['x']-(px+x0),n['z']-(pz+z0);length=math.hypot(dx,dz);dx/=length;dz/=length
  mx,mz=px+dx*14,pz+dz*14;level=max(-35,min(-8,floor+5));bed=level-5
  if refined:
   zz0,xx0=np.mgrid[:size,:size]
   near=((xx0-px)**2+(zz0-pz)**2)<65**2
   belowSea=np.arange(384)[None,None,:]<84
   a[aquatic & near[:,:,None] & belowSea]=8
   # Consolidated seabed around the cave aperture; loose marine sediment is
   # inappropriate on these exposed cavity shelves, including hanging layers.
   a[np.isin(a,[5,42]) & near[:,:,None] & belowSea]=6
  b=a.copy();zz,xx=np.mgrid[:size,:size];u=(xx-mx)*dx+(zz-mz)*dz;v=-(xx-mx)*dz+(zz-mz)*dx
  angle=np.arctan2(v,u);warp=1+.07*np.sin(angle*3+j['config']['seed']%19)+.04*np.cos(angle*5)
  q=np.sqrt((u/29)**2+(v/24)**2)/warp
  cavity=air[:,:,level+64];pool=(q<1)&cavity
  # Do not turn existing pillars into a lake or overwrite the sea itself.
  if pool.sum()<400 or not pool[pz,px]:rejected.append([int(n['x']),int(n['z']),'insufficient lake floor']);continue
  for z,x in np.argwhere((q<1.4)&cavity):
   depth=max(1,int(5*(1-min(1,q[z,x])**2)))
   if pool[z,x]:
    bottom=level-depth;b[z,x,bottom+62:bottom+65]=2
    b[z,x,bottom+65:level+65]=8
   else:
    # A tapered rock sill contains the lake above the old low cave floor.
    solid=np.flatnonzero(~air[z,x,9:level+65]);original=int(solid[-1])+9-64 if len(solid) else -55
    t=max(0,min(1,(q[z,x]-1)/.4));strength=1-t*t*(3-2*t)
    h=int(math.ceil(original+(level+2-original)*strength))
    low=max(-55,min(original+1,bed-3));b[z,x,low+64:h+65]=2
  # Close other sea-facing leaks in this chamber while leaving a narrow inlet.
  # This makes a waterfall aperture, not an unsupported wall of sea sources.
  for z,x,k in np.argwhere(faces):
   b[z,x,k]=2
  fall=[]
  for z,x in np.argwhere(pool & (((xx-px)**2+(zz-pz)**2)<16)):
   ks=np.flatnonzero(faces[z,x]);
   if not len(ks):continue
   high=int(ks[-1])-64
   if high-level<8:continue
   if np.any(~air[z,x,level+65:high+65]):continue
   b[z,x,level+65:high+65]=126;fall.append([int(x)+x0,int(z)+z0,level+1,high])
  if not fall:rejected.append([int(n['x']),int(n['z']),'no continuous waterfall']);continue
  # Seal lake sides and bottom even where a pre-existing low tunnel crosses
  # the new pool edge. Only the lake's immediate contact shell is affected.
  lake=np.zeros_like(sea);lake[:,:,level+64]=pool
  water=(b==8)&(np.arange(384)[None,None,:]<=level+64)&(q[:,:,None]<1)
  for dz,dx,dy in [(1,0,0),(-1,0,0),(0,1,0),(0,-1,0),(0,0,-1)]:
   edge=np.roll(water,(dz,dx,dy),(0,1,2))&np.isin(b,[0,28,29,30,31,37,67,68,116,117,118,119,120,121,122,123,124,125]);b[edge]=2
  if refined:
   # Reserve a two-block buffer at active water and remove orphaned decorations.
   decor=np.isin(b,[37,67,68,120,121,122,123,124,125])
   wet=np.isin(b,[8,126]);nearwet=wet.copy()
   for dz,dx,dy in [(1,0,0),(-1,0,0),(0,1,0),(0,-1,0),(0,0,1),(0,0,-1)]:
    nearwet|=np.roll(wet,(dz,dx,dy),(0,1,2))
   b[decor&nearwet]=0
   # Never remove a containing wall: plants are nonstructural; the lake shell
   # was already sealed. Dripleaf goes on dry clay banks in the later pass.
   from bake_lush_pools import fit_pools
   ponds=fit_pools(b,x0,z0,j['config']['seed'],max_pools=8)
  else:ponds=[]
  a=base_original
  assert np.array_equal(a[:,:,:9],b[:,:,:9])
  # Extract compact column runs as the special structure's placement geometry.
  runs=[]
  for z,x in np.argwhere((a!=b).any(axis=2)):
   changed=np.flatnonzero(a[z,x]!=b[z,x]);i=0
   while i<len(changed):
    k=int(changed[i]);end=k;id=int(b[z,x,k]);i+=1
    while i<len(changed) and int(changed[i])==end+1 and b[z,x,changed[i]]==id:end+=1;i+=1
    runs.append([int(x)+x0,int(z)+z0,k-64,end-64,id])
  feature=dict(center=[int(n['x']),int(n['z'])],bounds=[x0,z0,x0+size-1,z0+size-1],lakeLevel=level,lakeColumns=int(pool.sum()),inlet=[px+x0,pz+z0],falls=fall,runs=runs,clayPools=ponds)
  np.savez_compressed(out/'fitted-volume.npz',before=a,after=b,x0=x0,z0=z0)
  features.append(feature)
  break # A rare landmark within this stone forest, not every coastal cavity.
 from bake_lush_pools import bake as bake_lush
 lush=bake_lush(source,out/'lush-pools',groups,features) if j['config']['cave'].get('habitatRefinement') else []
 plan=dict(lushPools=lush,algorithm='stone-forest-falls-v1',seed=j['config']['seed'],sourceSha256=identity(j),features=features)
 j['stoneForestGrotto']=plan;source.write_text(json.dumps(j,separators=(',',':')),'utf-8')
 report=dict(features=[{k:v for k,v in f.items() if k!='runs'} for f in features],rejected=rejected)
 (out/'plan.json').write_text(json.dumps(report,indent=2),'utf-8');print(json.dumps(report),flush=True)
if __name__=='__main__':bake(sys.argv[1],sys.argv[2])
