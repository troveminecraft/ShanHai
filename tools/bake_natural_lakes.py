"""Freeze sparse natural-basin water spans; never bake chunks or change ground heights."""
from pathlib import Path
from world_bounds import HALF, SIDE, CHUNKS
import json,subprocess,heapq,hashlib,argparse
import numpy as np
from large_lakes import bake_large
from seed_progress import add_arguments,from_arguments
P=Path(__file__).resolve().parents[1];O=P/'outputs/agent-a-0.6.24-r01'
exe=P/'build/windows/x64/release/native-worldgen-check.exe';source=P/'data/native-input.json'
def scan(x,z,width,step,path,progress=None):
 if width>10000 and width%10000==0:
  # Bounded tiles avoid constructing millions of Python row objects at once.
  # Keep the full 16-block sampling resolution for drainage continuity.
  from concurrent.futures import ThreadPoolExecutor
  n=width//step;cache=path.with_suffix('.npy')
  result=np.lib.format.open_memmap(cache,mode='w+',dtype=np.float64,shape=(n,n,10))
  def tile(q):
   tx,tz=q;tp=path.with_name(f'{path.stem}-tile-{tx}-{tz}.json')
   rows=scan(x+tx,z+tz,10000,step,tp).reshape(10000//step,10000//step,10)
   result[tz//step:(tz+10000)//step,tx//step:(tx+10000)//step]=rows
   print('coarse tile',tx,tz,flush=True)
  tiles=[(tx,tz) for tz in range(0,width,10000) for tx in range(0,width,10000)]
  with ThreadPoolExecutor(max_workers=2) as pool:
   for done,_ in enumerate(pool.map(tile,tiles),1):
    if progress:progress(done/len(tiles))
  result.flush();return result.reshape(-1,10)
 q=dict(scan=True,x=int(x),z=int(z),width=width,step=step,output=str(path))
 subprocess.run([str(exe),str(source)],input=json.dumps(q)+'\n',capture_output=True,text=True,check=True)
 return np.array(json.loads(path.read_text('utf-8')))
def fill(h,progress=None):
 n,m=h.shape;out=h.copy();seen=np.zeros(h.shape,bool);heap=[]
 for z in range(n):
  for x in [0,m-1]:heapq.heappush(heap,(int(h[z,x]),z,x));seen[z,x]=True
 for x in range(1,m-1):
  for z in [0,n-1]:heapq.heappush(heap,(int(h[z,x]),z,x));seen[z,x]=True
 done=0
 while heap:
  v,z,x=heapq.heappop(heap)
  done+=1
  if progress and done%8192==0:progress(done/(n*m))
  for dz,dx in [(1,0),(-1,0),(0,1),(0,-1)]:
   nz,nx=z+dz,x+dx
   if 0<=nz<n and 0<=nx<m and not seen[nz,nx]:
    seen[nz,nx]=True;vv=max(v,int(h[nz,nx]));out[nz,nx]=vv;heapq.heappush(heap,(vv,nz,nx))
 return out
if __name__=='__main__':
 args=argparse.ArgumentParser();args.add_argument('--source',type=Path,default=source);args.add_argument('--output',type=Path,default=O);add_arguments(args);args=args.parse_args();source=args.source.resolve();O=args.output.resolve()
 progress=from_arguments(args);progress.update('正在采样地表与湖泊',0.)
 O.mkdir(parents=True,exist_ok=True)
 j=json.loads(source.read_text('utf-8'));j.pop('aridLandforms',None);j['naturalLakes']=[];j.pop('overhangCliffs',None);j.pop('rivers',None);j.pop('riverSource',None);j.get('seedManifest',{}).pop('riversReady',None);j.pop('naturalLakeSource',None);source.write_text(json.dumps(j,ensure_ascii=False,separators=(',',':')),encoding='utf-8',newline='\n')
 # Hydrology must use real 16-block samples. Repeating a coarser grid invents
 # flat cells and can route water across ridges. Always write this seed's grid
 # so the river baker cannot accidentally consume a previous bake's file.
 a=scan(-HALF,-HALF,SIDE,16,O/'coarse-surface.json',lambda f:progress.update(f'地表采样：{round(f*100)}%',.65*f))
 h=a[:,2].astype(int).reshape(CHUNKS,CHUNKS);f=fill(h,lambda f:progress.update('正在识别封闭湖泊盆地',.65+.09*f))
 zz,xx=np.where((f-h>=3)&(h>=135)&(f<=260));c=sorted([(int(f[z,x]-h[z,x]),int(x)*16-HALF,int(z)*16-HALF) for z,x in zip(zz,xx)],reverse=True);candidates=[]
 for dep,x,z in c:
  if abs(x)>(HALF-296) or abs(z)>(HALF-296) or any((x-u)**2+(z-v)**2<180**2 for _,u,v in candidates):continue
  row=a[((z+HALF)//16)*CHUNKS+(x+HALF)//16]
  if row[6]>.01 or row[7]>.01 or row[9]>0:continue
  candidates.append((dep,x,z))
  # Keep the seed bake bounded when the playable square is enlarged. The
  # basin test itself remains physical; this only limits how many candidates
  # are deeply re-sampled before accepting the strongest ones.
  if len(candidates)>=64:break
 print('candidates',candidates,flush=True)
 progress.update('正在分析大型湖泊',.74)
 lakes,details,occupied=bake_large(exe,source,a,O,fill)
 for index,(_,cx,cz) in enumerate(candidates):
  progress.update(f'湖泊盆地：{index+1}/{len(candidates)}',.80+.18*index/max(1,len(candidates)))
  n=161;x0=cx-80;z0=cz-80;b=scan(x0,z0,n,1,O/f'basin-{index}.json').reshape(n,n,-1);h=b[:,:,2].astype(int);f=fill(h)
  local=h[56:105,56:105];sz,sx=np.unravel_index(local.argmin(),local.shape);sz=int(sz)+56;sx=int(sx)+56
  floor=int(h[sz,sx]);spill=int(f[sz,sx]);level=min(spill-1,floor+14)
  if level-floor<3 or level<140:continue
  wet={(sz,sx)};queue=[(sz,sx)];bad=False
  for z,x in queue:
   if z==0 or x==0 or z==n-1 or x==n-1:bad=True;break
   if b[z,x,6]>.01 or b[z,x,7]>.01 or b[z,x,8]>-32768 or b[z,x,9]>0:bad=True;break
   for dz,dx in [(1,0),(-1,0),(0,1),(0,-1)]:
    k=z+dz,x+dx
    if k not in wet and h[k]<level:wet.add(k);queue.append(k)
  if bad or not 160<=len(wet)<=6000:continue
  edge={(z+dz,x+dx) for z,x in wet for dz,dx in [(1,0),(-1,0),(0,1),(0,-1)] if (z+dz,x+dx) not in wet}
  if any(b[z,x,8]>-32768 for z,x in edge):continue  # An alcove in the wall can drain a height-closed basin.
  bank=set()
  for z,x in wet:
   for dz in range(-2,3):
    for dx in range(-2,3):
     k=z+dz,x+dx
     if 0<=k[0]<n and 0<=k[1]<n and k not in wet and level<=h[k]<=level+2:bank.add(k)
  rows=[]
  for group,t in [(wet,1),(bank,0)]:
   for z in sorted(set(z for z,x in group)):
    xs=sorted(x for zz,x in group if zz==z);start=last=xs[0]
    for x in xs[1:]+[100000]:
     if x!=last+1:rows.append([z0+z,x0+start,x0+last,t]);start=x
     last=x
  if any(any(a<=b1 and b>=a1 for a,b in occupied.get(z,())) for z,a1,b1,_ in rows):continue
  for z,a1,b1,_ in rows:occupied.setdefault(z,[]).append((a1,b1))
  lake=dict(level=level,rows=rows,center=[x0+sx,z0+sz],floor=floor,spill=spill,area=len(wet))
  lakes.append(lake);details.append(dict(index=index,center=lake['center'],level=level,floor=floor,spill=spill,area=len(wet),bankColumns=len(bank),bounds=[x0,z0,n]));print('accepted',details[-1],flush=True)
 # Some valid seeds have no suitable closed basins; never invent water to meet a quota.
 progress.update('正在保存湖泊与海岸数据',.99)
 j['naturalLakes']=lakes
 if 'seedManifest' in j:j['seedManifest']['ready']=True
 identity={k:v for k,v in j.items() if k not in ['lavaFlows','iceRegions','stoneForestGrotto','cavernPlan','version','naturalLakes','naturalLakeSource','rivers','riverSource','overhangCliffs']}
 j['naturalLakeSource']=hashlib.sha256(json.dumps(identity,sort_keys=True,separators=(',',':')).encode()).hexdigest()
 source.write_text(json.dumps(j,ensure_ascii=False,separators=(',',':')),encoding='utf-8',newline='\n')
 coasts=sorted([r.tolist() for r in a if r[4]>.25 and 35<r[2]<90],key=lambda r:-r[4]);(O/'coast-candidates.json').write_text(json.dumps(coasts[:1000]),encoding='utf-8')
 (O/'lake-bake.json').write_text(json.dumps(dict(lakes=details,coastSamples=len(coasts),source=j['naturalLakeSource']),indent=2),encoding='utf-8')
 print('lakes',len(lakes),'coast candidates',len(coasts),flush=True)
 progress.update('湖泊分析完成',1.)
