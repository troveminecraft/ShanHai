"""Only extend an existing lava pool through a naturally low outlet."""
from pathlib import Path
from world_bounds import HALF
import json,hashlib,math,sys
P=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(P/'tests'));from verify_water import run
DIRS=[(0,0),(1,0),(-1,0),(0,1),(0,-1)]
def identity(j):return hashlib.sha256(json.dumps({k:v for k,v in j.items() if k not in ('lavaFlows','iceRegions','version')},sort_keys=True,separators=(',',':')).encode()).hexdigest()
def trace(path,cache,crest):
 first=cache[path[0][:2]];pool=first['lava']
 if pool==-32768 or first['height']>=pool:return None
 level=pool;anchor=None;center=[];levels={};flat=0;inlet=None
 for i,(x,z,r) in enumerate(path):
  s=cache[x,z]
  if s['volcanic']<.99 or s['river'] or s['lake'] or s['water']>=s['height']:break
  if s['lava']==pool and s['height']<pool:
   if inlet is not None:break
   anchor=[x,z,pool];continue
  if anchor is None:return None
  level=min(level,s['height']+1)
  if s['height']-(level-1)>2:return None
  flat=flat+1 if center and level==center[-1][2] else 0
  if i>crest and flat>3:break
  if inlet is None:inlet=(x,z)
  center.append([x,z,level])
  for dx,dz in DIRS:
   p=x+dx,z+dz;b=cache[p]
   if b['volcanic']<.99 or b['river'] or b['lake'] or b['water']>=b['height']:continue
   if b['lava']!=-32768 and b['height']<b['lava']:continue  # Preserve the real pool.
   if (dx or dz) and (b['height']>level+1 or b['height']<level-3):continue
   levels[p]=min(levels.get(p,level),level)
 if not center or len(center)<65 or pool-center[-1][2]<35:return None
 columns=[]
 for (x,z),level in sorted(levels.items()):
  ground=cache[x,z]['height'];bed=min(ground,level-1)
  if ground-bed>2:return None
  top=max([level]+[levels.get((x+dx,z+dz),level) for dx,dz in DIRS[1:]])
  if (x,z)==inlet:top=max(top,pool)
  clear=max(ground,top)
  if bed<32 or top>300 or clear-bed>100:return None
  columns.append([x,z,bed,top,clear,ground])
 if inlet not in levels or abs(inlet[0]-anchor[0])+abs(inlet[1]-anchor[1])!=1:return None
 return dict(poolAnchor=anchor,path=center,columns=columns,maxCut=max(c[5]-c[2] for c in columns))
def bake(source,out):
 source=Path(source);out=Path(out);out.mkdir(parents=True,exist_ok=True)
 j=json.loads(source.read_text('utf-8'));j.pop('lavaFlows',None);source.write_text(json.dumps(j,separators=(',',':')),'utf-8')
 E=P/'build/windows/x64/release/native-worldgen-check.exe';seed=j['config']['seed'];flows=[];rejected=[];audit=[]
 def rand(v,salt):return int.from_bytes(hashlib.sha256(f"{seed}:{v['x']}:{v['z']}:{salt}".encode()).digest()[:4],'little')/2**32
 for v in run(E,source,[{'volcanoes':True}])[0]:
  if abs(v['x'])+v['outer']>=HALF-10 or abs(v['z'])+v['outer']>=HALF-10 or rand(v,1)>=.28:continue
  cache={}
  def sample(points):
   q=sorted(set(points)-cache.keys())
   if q:cache.update(zip(q,run(E,source,[[x+32768,z+32768] for x,z in q])))
  origin=round(v['x']),round(v['z']);sample([origin]);pool=cache[origin]['lava'];rays=[]
  for k in range(48):
   angle=rand(v,2)*math.tau+k*math.tau/48
   pts=[(round(v['x']+r*math.cos(angle)),round(v['z']+r*math.sin(angle))) for r in range(int(v['crater']*.2),math.ceil(v['crater']*1.5),2)]
   rays.append((angle,pts))
  sample(p for _,pts in rays for p in pts)
  ranked=sorted((max(cache[p]['height'] for p in pts),angle) for angle,pts in rays)
  audit.append(dict(volcano=list(origin),originalPool=pool,lowestSampledRim=ranked[0][0],gap=ranked[0][0]-pool))
  chosen=None;tried=[]
  for height,angle in ranked:
   if pool==-32768 or height>pool+1:continue  # At most two blocks below the existing rim.
   if any(abs(math.remainder(angle-b,math.tau))<.3 for b in tried):continue
   tried.append(angle)
   if len(tried)>6:break
   path=[]
   for r in range(int(v['crater']*.2),math.ceil(v['outer']*.62)):
    bend=.018*math.sin(max(0,r-v['crater'])/43)*max(0,min(1,(r-v['crater'])/30))
    x,z=round(v['x']+r*math.cos(angle+bend)),round(v['z']+r*math.sin(angle+bend))
    if path and (x,z)==path[-1][:2]:continue
    if path and x!=path[-1][0] and z!=path[-1][1]:path.append((x,path[-1][1],r))
    path.append((x,z,r))
   sample((x+dx,z+dz) for x,z,_ in path for dx,dz in DIRS)
   crest=max(range(len(path)),key=lambda i:cache[path[i][:2]]['height'] if path[i][2]<=v['crater']*1.5 else -10000)
   chosen=trace(path,cache,crest)
   if chosen:chosen['volcano']=list(origin);break
  if chosen:flows.append(chosen)
  else:rejected.append([*origin,'no naturally connected shallow outlet'])
 j['lavaFlows']=dict(algorithm='volcanic-overflows-v4',seed=seed,chance=.28,maxCut=2,sourceSha256=identity(j),flows=flows)
 source.write_text(json.dumps(j,separators=(',',':')),'utf-8')
 report=dict(flows=len(flows),columns=sum(len(f['columns']) for f in flows),rejected=rejected,audit=audit,maximumCut=max((f['maxCut'] for f in flows),default=0))
 (out/'lava-bake.json').write_text(json.dumps(report,indent=2),'utf-8');print(report,flush=True)
if __name__=='__main__':bake(sys.argv[1],sys.argv[2])
