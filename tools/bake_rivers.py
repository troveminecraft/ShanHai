"""Seed-derived connected drainage descriptions. No chunk or voxel pre-generation."""
from pathlib import Path
from world_bounds import HALF, SIDE, CHUNKS
import argparse,json,heapq,math,hashlib,subprocess,collections
import numpy as np
from route_forest_rivers import route_forest
from lake_outlets import add_outlets
from fit_lake_banks import fit_lake_banks
from mainstem_banks import fit_mainstems, describe_mainstems, fit_mainstem_mouths, normalize_flow
from tributary_capture import capture_tributaries
from headwater_sources import select_sources, validate_final_sources, POLICY as HEADWATER_POLICY
from seed_progress import add_arguments,from_arguments
P=Path(__file__).resolve().parents[1]

def identity(j):
 base={k:v for k,v in j.items() if k not in ['lavaFlows','iceRegions','version','naturalLakes','naturalLakeSource','rivers','riverSource','overhangCliffs']}
 return hashlib.sha256(json.dumps(base,sort_keys=True,separators=(',',':')).encode()).hexdigest()

def bake(source,coarse,out,progress=None):
 if progress:progress.update('正在建立江河汇水网络',0.)
 j=json.loads(source.read_text('utf-8'));j.pop('aridLandforms',None);a=np.load(coarse.with_suffix('.npy'),mmap_mode='r').reshape(-1,10) if coarse.with_suffix('.npy').exists() else np.array(json.loads(coarse.read_text('utf-8')));n=CHUNKS;assert a.shape==(n*n,10)
 h=a[:,2].reshape(n,n);stone=a[:,6].reshape(n,n);volcano=a[:,9].reshape(n,n)
 blocked=(stone>.001)|(volcano>0);expanded=blocked.copy()
 for dz in range(-2,3):
  for dx in range(-2,3):expanded|=np.roll(np.roll(blocked,dz,0),dx,1)
 blocked=expanded;blocked[:6]=True;blocked[-6:]=True;blocked[:,:6]=True;blocked[:,-6:]=True
 # Every actual marine water column is a receiver, including the Y=29/30
 # shelf. Routing across that shelf must not inflate inland river lengths.
 sink=np.full((n,n),-1,dtype=np.int32);sink[(h<31)&~blocked]=0
 lakelevels={}
 for i,lake in enumerate(j['naturalLakes'],1):
  lakelevels[i]=lake['level']
  for z,x0,x1,t in lake['rows']:
   if t!=1 or z%16:continue
   for x in range(math.ceil(x0/16)*16,x1+1,16):
    gx,gz=(x+HALF)//16,(z+HALF)//16
    if not blocked[gz,gx]:sink[gz,gx]=i
 routeh=h.copy()
 for i,level in lakelevels.items():routeh[sink==i]=level
 dist=np.full((n,n),np.inf);parent=np.full(n*n,-1,dtype=np.int32);root=np.full(n*n,-1,dtype=np.int32);heap=[]
 for z,x in zip(*np.where(sink>=0)):
  idx=int(z*n+x);dist[z,x]=0;root[idx]=int(sink[z,x]);heap.append((0.,idx))
 heapq.heapify(heap);order=[]
 reachable=int((~blocked).sum())
 offsets=[(-1,0,16),(1,0,16),(0,-1,16),(0,1,16),(-1,-1,22.627),(1,-1,22.627),(-1,1,22.627),(1,1,22.627)]
 while heap:
  cost,i=heapq.heappop(heap);z,x=divmod(i,n)
  if cost!=dist[z,x]:continue
  order.append(i)
  if progress and len(order)%8192==0:
   progress.update(f'汇水路径：{len(order)}/{reachable}',.35*len(order)/max(1,reachable))
  for dx,dz,length in offsets:
   xx,zz=x+dx,z+dz
   if not(0<=xx<n and 0<=zz<n) or blocked[zz,xx]:continue
   if dx and dz and (blocked[z,xx] or blocked[zz,x]):continue
   uphill=max(0.,float(routeh[z,x]-routeh[zz,xx]));nc=cost+length+35*uphill*uphill
   if nc<dist[zz,xx]:
    dist[zz,xx]=nc;k=zz*n+xx;parent[k]=i;root[k]=root[i];heapq.heappush(heap,(nc,k))
 if progress:progress.update('正在计算河流流量与长度',.35)
 acc=np.ones(n*n,dtype=np.int64)
 for i in reversed(order):
  if parent[i]>=0:acc[parent[i]]+=acc[i]
 def xy(i):z,x=divmod(int(i),n);return (x*16-HALF,z*16-HALF)
 def height(i):z,x=divmod(int(i),n);return float(h[z,x])
 def rand(i):return int(hashlib.sha256((j['seedManifest']['terrainSeed']+':river:'+str(i)).encode()).hexdigest()[:8],16)/2**32
 # Evaluate real land paths before small tributaries occupy their shared
 # downstream nodes. No coordinate presets or loops to manufacture length.
 lengths=np.zeros(n*n)
 for i in order:
  if parent[i]>=0:lengths[i]=lengths[parent[i]]+math.dist(xy(i),xy(parent[i]))
 main_candidates=[]
 for done,i in enumerate(order):
  if progress and done%16384==0:progress.update('正在筛选江河路径',.38+.12*done/max(1,len(order)))
  if parent[i]<0 or root[i]<0 or acc[i]<12 or not 90<=height(i)<=220 or lengths[i]<1200:continue
  path=[];k=i
  while parent[k]>=0 and len(path)<700:path.append(k);k=int(parent[k])
  target=31 if root[k]==0 else lakelevels[int(root[k])];w=height(i)-1;cut=0;exposed=0
  for v in path:w=max(target,min(w,height(v)-1));cut=max(cut,height(v)-w);exposed=max(exposed,w-height(v))
  if len(path)<700 and cut<=16 and exposed<=1 and w-target<=24:main_candidates.append(i)
 main_candidates.sort(key=lambda i:-lengths[i])
 candidates=[i for i in order if 85<=height(i)<=245 and acc[i]>=12 and dist.flat[i]>400 and root[i]>=0]
 # Both river classes use the same natural source rules. Long lowland routes
 # no longer bypass the climate/elevation check just to satisfy mainstem length.
 uncut={k:v for k,v in j.items() if k not in ['lavaFlows','iceRegions','rivers','riverSource','overhangCliffs']}
 out.mkdir(parents=True,exist_ok=True);scratch=out/'river-base-input.json'
 scratch.write_text(json.dumps(uncut,separators=(',',':')),encoding='utf-8')
 if progress:progress.update('正在筛选湿润高地河源',.50)
 habitats=select_sources(j,main_candidates+candidates,acc,scratch,out,
                        (lambda done,total:progress.update(f'河源筛选：{done}/{total}',.50+.25*done/max(1,total))) if progress else None)
 main_candidates=[i for i in main_candidates if i in habitats]
 candidates=[i for i in candidates if i in habitats]
 candidates.sort(key=lambda i:-(height(i)*acc[i]**.18*(.8+.4*rand(i))))
 # Preserve river density when expanding the playable area. Source checks,
 # erosion depth and receiving-water requirements remain the same.
 areaScale=(SIDE/8192)**2;routeBudget=min(max(216,round(216*(SIDE/20000)**2)),max(36,round(36*areaScale)))
 routeBudget=round(routeBudget*1.45)
 independentBudget=max(24,round(24*areaScale*1.20));mainBudget=max(3,round(3*areaScale))
 sourceTiles=collections.Counter();tileBudget=min(9,max(4,math.ceil(routeBudget/36)))
 if progress:progress.update('正在连接江河与支流',.75)
 network={};routes=[];sources=[];rejected=0
 main_sources=[]
 for phase,sourceid in [(0,i) for i in main_candidates+candidates]+[(1,i) for i in candidates]:
  sx,sz=xy(sourceid)
  tributary=phase==1 or len(routes)>=independentBudget
  tile=(sx//2048,sz//2048)
  if sourceTiles[tile]>=tileBudget:continue
  if any((sx-x)**2+(sz-z)**2<(140 if tributary else 340)**2 for x,z in sources):continue
  path=[];i=sourceid
  while i>=0 and i not in network and parent[i]>=0 and len(path)<700:
   path.append(i);i=int(parent[i])
  if i<0 or len(path)<(8 if tributary else 18) or len(path)>=700:continue
  if tributary and (i not in network or network[i]['parent']<0 or network[i]['water']<40):continue
  target=i;targetwater=network[i]['water'] if i in network else 31 if root[i]==0 else lakelevels[int(root[i])]
  waters=[];w=height(sourceid)-1
  for k in path:
   w=max(float(targetwater),min(w,height(k)-1));waters.append(w)
  is_main=sourceid in main_candidates and len(main_sources)<mainBudget and sum(math.dist(xy(a),xy(b)) for a,b in zip(path,path[1:]+[target]))>=1200
  if any(v-height(k)>2 for k,v in zip(path,waters)) or max(height(k)-v for k,v in zip(path,waters))>(16 if is_main else 12) or waters[-1]-targetwater>24:
   rejected+=1;continue
  if waters[0]-targetwater<16:continue
  if target not in network:network[target]={'water':float(targetwater),'parent':-1,'flow':0}
  for k,v,nextid in zip(path,waters,path[1:]+[target]):network[k]={'water':float(v),'parent':nextid,'flow':0}
  if is_main:
   main_sources.append(sourceid)
   for k in path+[target]:network[k]['main']=True;network[k]['catchment']=int(acc[k])*256
  flow=max(1,min(4,math.sqrt(float(acc[sourceid]))/8));i=sourceid
  while i>=0:network[i]['flow']+=flow;i=network[i]['parent']
  routes.append({'source':sourceid,'join':target,'receiver':'river' if network[target]['parent']>=0 else 'ocean' if root[target]==0 else 'lake','sink':int(root[target]),'path':path+[target]});sources.append((sx,sz))
  sourceTiles[tile]+=1
  if len(routes)>=routeBudget:break
 # A seed with no valid receiving route may legitimately contain no rivers.
 # Relax single-flow bends, keeping sources, junctions and mouths exact.
 upstream={i:[] for i in network}
 for i,v in network.items():
  if v['parent']>=0:upstream[v['parent']].append(i)
 sourceIds={r["source"] for r in routes}
 for i in sourceIds:network[i]["source"]=True
 positions={i:xy(i) for i in network}
 for i,v in network.items():
  if i not in sourceIds and len(upstream[i])==1 and v['parent']>=0:
   x,z=xy(i);ux,uz=xy(upstream[i][0]);dx,dz=xy(v['parent']);positions[i]=(.75*x+.125*(ux+dx),.75*z+.125*(uz+dz))
 # Broad seeded bends remove straight routing-grid runs. Keep joins/mouths
 # exact and accept bends only where the actual uncut ground can contain them.
 anchors=[positions[i] for i,v in network.items() if i in sourceIds or len(upstream[i])!=1 or v['parent']<0]
 proposals={};phase=rand(991)*math.tau
 for i,(x,z) in positions.items():
  distance=min((math.hypot(x-a,z-b) for a,b in anchors),default=0)
  fade=min(1,distance/64);fade=fade*fade*(3-2*fade)
  dx=12*math.sin(z/53+phase)+4*math.sin(x/83-phase);dz=12*math.sin(x/61-phase)+4*math.sin(z/97+phase)
  scale=min(1,12/max(.001,math.hypot(dx,dz)))*fade
  if network[i].get('main'):
   dx=16*math.sin(z/160+phase);dz=16*math.sin(x/190-phase);scale=fade
  proposals[i]=(x+dx*scale,z+dz*scale)
 uncut={k:v for k,v in j.items() if k not in ['lavaFlows','iceRegions','rivers','riverSource','overhangCliffs']};scratch=out/'river-base-input.json';out.mkdir(parents=True,exist_ok=True);scratch.write_text(json.dumps(uncut,separators=(',',':')),encoding='utf-8')
 keys=list(proposals);queries=[[round(proposals[i][0])+32768,round(proposals[i][1])+32768] for i in keys]
 proc=subprocess.run([str(P/'build/windows/x64/release/native-worldgen-check.exe'),str(scratch)],input=''.join(json.dumps(q)+'\n' for q in queries),text=True,capture_output=True,check=True)
 for i,row in zip(keys,map(json.loads,proc.stdout.splitlines())):
  network[i]['humidity']=row['H']
  if network[i]['water']-2<=row['height']<=network[i]['water']+12 and row['stone']==0 and row['volcanic']==0:positions[i]=proposals[i]
 # A bend adjacent to a fixed source/junction can stretch an edge beyond
 # the native segment budget. Reject that local warp instead of failing an
 # otherwise valid seed or weakening the runtime validation limit.
 while True:
  bad=[(i,v['parent']) for i,v in network.items() if v['parent']>=0 and math.dist(positions[i],positions[v['parent']])>39]
  if not bad:break
  for i,k in bad:positions[i]=xy(i);positions[k]=xy(k)
 # Fit the water profile to actual banks after bend displacement. Coarse
 # center heights alone can place a river above the downhill side of a slope.
 # Distance from a headwater increases downstream through confluences.
 pending={i:len(upstream[i]) for i in network};queue=collections.deque(i for i in network if not pending[i])
 for v in network.values():v['age']=0.
 while queue:
  i=queue.popleft();nxt=network[i]['parent']
  if nxt>=0:
   network[nxt]['age']=max(network[nxt]['age'],network[i]['age']+math.dist(positions[i],positions[nxt]));pending[nxt]-=1
   if not pending[nxt]:queue.append(nxt)
 def radius(a):
  age=min(1.,a['age']/144);age=age*age*(3-2*age)
  low=max(0.,min(1.,(65-a['water'])/34));low=low*low*(3-2*low)
  mature=2.3+min(4.,math.sqrt(a['flow'])*.65)+low*min(5.,math.sqrt(a['flow'])*1.1)
  return .85+(mature-.85)*age
 if progress:progress.update('正在衔接湖岸与江岸',.82)
 fit_lake_banks(network,positions,upstream,radius,scratch,out)
 widths=fit_mainstems(network,positions,upstream,radius,scratch,out)
 narrow_radius=radius
 def radius(a):return widths.get(id(a),narrow_radius(a))
 if progress:progress.update('正在计算河岸侵蚀与水位',.87)
 bankkeys=[];bankqueries=[]
 for i,v in network.items():
  if v['parent']<0:continue
  x,z=positions[i];xx,zz=positions[v['parent']];dx,dz=xx-x,zz-z;length=math.hypot(dx,dz);nx,nz=-dz/length,dx/length
  for side in [-1,1]:
   for t in [-2,0,2]:
    bankkeys.append(i);bankqueries.append([round(x+nx*(radius(v)+2.5)*side+dx/length*t)+32768,round(z+nz*(radius(v)+2.5)*side+dz/length*t)+32768])
 proc=subprocess.run([str(P/'build/windows/x64/release/native-worldgen-check.exe'),str(scratch)],input=''.join(json.dumps(q)+'\n' for q in bankqueries),text=True,capture_output=True,check=True)
 for i,row in zip(bankkeys,map(json.loads,proc.stdout.splitlines())):
  network[i]['water']=min(network[i]['water'],max(31,row['height']-1))
 # Receiving bodies retain their real level. Propagate a lowered reach only
 # downstream, keeping junctions identical and forbidding uphill water steps.
 for i,v in network.items():
  dest=i
  while network[dest]['parent']>=0:dest=network[dest]['parent']
  v['water']=max(v['water'],network[dest]['water'])
 pending={i:len(upstream[i]) for i in network};queue=collections.deque(i for i in network if not pending[i])
 while queue:
  i=queue.popleft();nxt=network[i]['parent']
  if nxt>=0:
   network[nxt]['water']=min(network[nxt]['water'],network[i]['water']);pending[nxt]-=1
   if not pending[nxt]:queue.append(nxt)
 segments=[];waterfalls=[]
 for i,v in network.items():
  if v['parent']<0:continue
  dest=network[v['parent']];x,z=positions[i];xx,zz=positions[v['parent']]
  seg=[x,z,xx,zz,v['water'],dest['water'],radius(v),radius(dest)];segments.append(seg)
  if v['water']-dest['water']>5:waterfalls.append(seg)
 descriptions=describe_mainstems(network,positions,upstream,main_sources,radius)
 j['rivers']={'algorithm':'drainage-tree-v8-inland-mainstems','cutBanks':10,'naturalBankSlope':1,'sectionProfiles':1,'confluenceProfiles':1,'capturePolicy':3,'treeSubstrates':True,'segments':segments,'mainstems':descriptions,'routes':[dict(source=positions[r['source']],join=positions[r['join']],receiver=r['receiver'],sink=r['sink'],nodes=len(r['path'])) for r in routes]}
 j['rivers']['densityPolicy']={'version':2,'routeMultiplier':1.45,'independentMultiplier':1.20,'sourceSpacing':340,'tributarySpacing':140,'tileLimit':tileBudget}
 j['rivers']['headwaterPolicy']=HEADWATER_POLICY
 j['rivers']['headwaters']=[dict(habitats[r['source']],selectionSource=list(xy(r['source'])),source=list(positions[r['source']])) for r in routes]
 if progress:progress.update('正在衔接江河入海与汇流口',.93)
 route_forest(j,scratch,out)
 fit_mainstem_mouths(j,scratch,out)
 add_outlets(j,scratch,out)
 normalize_flow(j,out)
 from headwater_runs import bake_headwater_runs
 bake_headwater_runs(j)
 capture_tributaries(j,scratch,out)
 normalize_flow(j,out)
 from headwater_runs import bake_headwater_runs
 bake_headwater_runs(j)
 validate_final_sources(j,scratch)
 j['seedManifest']['riversReady']=True;j['naturalLakeSource']=identity(j)
 from fluvial_landforms import bake as bake_landforms
 if progress:progress.update('正在完成江河侵蚀与堆积',.98)
 bake_landforms(j,out)
 j['riverSource']=hashlib.sha256(json.dumps({'base':j['naturalLakeSource'],'lakes':j['naturalLakes'],'rivers':j['rivers']},sort_keys=True,separators=(',',':')).encode()).hexdigest()
 source.write_text(json.dumps(j,separators=(',',':')),encoding='utf-8')
 report={'rivers':len(j['rivers']['routes']),'segments':len(j['rivers']['segments']),'receivers':{k:sum(r['receiver']==k for r in j['rivers']['routes']) for k in ['river','lake','ocean']},'waterfalls':waterfalls,'routes':j['rivers']['routes'],'rejected':rejected,'junctions':[dict(x=positions[i][0],z=positions[i][1],water=v['water']) for i,v in network.items() if len(upstream[i])>1]}
 out.mkdir(parents=True,exist_ok=True);(out/'river-plan.json').write_text(json.dumps(report,indent=2));print(json.dumps({k:v for k,v in report.items() if k not in ['lavaFlows','iceRegions','waterfalls','routes','junctions']}),flush=True)
 if progress:progress.update('江河数据准备完成',1.)
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);p.add_argument('--coarse',type=Path,required=True);p.add_argument('--output',type=Path,required=True);add_arguments(p);a=p.parse_args();bake(a.source.resolve(),a.coarse.resolve(),a.output.resolve(),from_arguments(a))
