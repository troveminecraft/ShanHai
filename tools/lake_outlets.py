"""Conservative lake outlets: short natural saddles to an existing lower river."""
from pathlib import Path
import json,math,heapq,subprocess
P=Path(__file__).resolve().parents[1]

def add_outlets(j,base,out):
 exe=P/'build/windows/x64/release/native-worldgen-check.exe'
 def query(q):
  r=subprocess.run([str(exe),str(base)],input=''.join(json.dumps(x)+'\n' for x in q),text=True,capture_output=True,check=True)
  return list(map(json.loads,r.stdout.splitlines()))
 incoming={r['sink'] for r in j['rivers']['routes'] if r['receiver']=='lake'}
 vertices={tuple(s[:2]):s[4] for s in j['rivers']['segments']}
 outlets=[];rejected=[]
 # Inspect the largest new receiving bodies first. An enclosed lake without
 # a shallow natural saddle remains enclosed, rather than cutting a deep canal.
 for index in sorted(incoming,key=lambda i:(not j['naturalLakes'][i-1].get('large',False),-j['naturalLakes'][i-1]['level'],i)):
  if len(outlets)>=3:break
  lake=j['naturalLakes'][index-1];level=lake['level'];cx,cz=lake['center']
  wet={(x,z) for z,x0,x1,t in lake['rows'] if t==1 for x in range(x0,x1+1)}
  lakeArea={(x,z) for z,x0,x1,t in lake['rows'] for x in range(x0,x1+1)}
  starts=sorted(p for p in wet if p[0]%4==0 and p[1]%4==0 and
                any((p[0]+dx,p[1]+dz) not in wet for dx,dz in [(4,0),(-4,0),(0,4),(0,-4)]))
  saddle={tuple(p[:2]):p[2] for p in lake.get('outletStarts',[])}
  if saddle:starts=sorted(saddle)
  # Measure outlet reach from the shoreline, not the deepest point, which can
  # lie hundreds of blocks away from the exit of an elongated large basin.
  targets=[]
  for p,w in vertices.items():
   if not 8<=level-w<=100 or not starts:continue
   distance=min(math.dist(s,p) for s in starts)
   if 32<distance<640:targets.append((distance,p,w))
  targets.sort()
  if not starts or not targets:continue
  # Spread attempts so nearby vertices on the same unreachable bank do not
  # consume the entire search budget.
  goals=[]
  for _,p,w in targets:
   if all(math.dist(p,g[0])>80 for g in goals):goals.append((p,w))
   if len(goals)>=4:break
  attempts=[]
  for goal,goalwater in goals:
   selected=[]
   for start in sorted(starts,key=lambda p:(math.dist(p,goal)+12*(saddle.get(p,level)-level),p)):
    if all(math.dist(start,s)>80 for s in selected):selected.append(start);attempts.append((goal,goalwater,start))
    if len(selected)>=(2 if saddle else 1):break
  success=False
  for goal,goalwater,start in attempts:
   end=tuple(round(v/4)*4 for v in goal)
   x0=math.floor((min(start[0],end[0])-40)/4)*4;z0=math.floor((min(start[1],end[1])-40)/4)*4
   size=math.ceil((max(abs(end[0]-start[0]),abs(end[1]-start[1]))+84)/4)*4
   if max(abs(x0),abs(z0),abs(x0+size),abs(z0+size))>4000:continue
   f=out/'outlet-corridor.json'
   query([dict(scan=True,x=x0,z=z0,width=size,step=4,output=str(f))]);grid={tuple(r[:2]):r for r in json.loads(f.read_text())}
   def valid(p):
    r=grid.get(p)
    return r is not None and r[6]<.001 and r[7]<.001 and r[9]==0 and (not r[5] or p in lakeArea) and r[2]<=level+6
   if not valid(start) or not valid(end):continue
   heap=[(math.dist(start,end),0.,start)];cost={start:0.};prev={};closed=set();waterAt={start:float(level)}
   while heap:
    _,g,u=heapq.heappop(heap)
    if g!=cost[u] or u in closed:continue
    closed.add(u)
    if u==end:break
    if len(closed)>40000:break
    for dx,dz in [(4,0),(-4,0),(0,4),(0,-4),(4,4),(4,-4),(-4,4),(-4,-4)]:
     v=u[0]+dx,u[1]+dz
     if v in closed or not valid(v):continue
     if dx and dz and (not valid((u[0]+dx,u[1])) or not valid((u[0],u[1]+dz))):continue
     h=grid[v][2];w=float(level) if v in wet else max(goalwater,min(waterAt[u],h-1))
     if w>waterAt[u] or h-w>8 or w-h>1 and v not in wet:continue
     penalty=.12*max(0,h-w-3)**2+.25*max(0,w-h)**2+2*max(0,h-grid[u][2])**2
     ng=g+math.hypot(dx,dz)*(1+penalty)
     if ng<cost.get(v,math.inf):cost[v]=ng;prev[v]=u;waterAt[v]=w;heapq.heappush(heap,(ng+math.dist(v,end),ng,v))
   if end not in closed:continue
   pts=[end]
   while pts[-1]!=start:pts.append(prev[pts[-1]])
   pts.reverse();pts=pts[max(i for i,p in enumerate(pts) if p in wet):]
   if pts[-1]!=goal:pts.append(goal)
   if len(pts)<4:continue
   # Dense centre and bank samples reject saddles that need deep artificial
   # canals. The lake surface and receiving river are immutable anchors.
   requests=[]
   for k,p in enumerate(pts):
    other=pts[min(k+1,len(pts)-1)] if k<len(pts)-1 else pts[k-1]
    dx,dz=other[0]-p[0],other[1]-p[1];length=math.hypot(dx,dz)
    requests.append([round(p[0])+32768,round(p[1])+32768])
    for side in [-1,1]:requests.append([round(p[0]-dz/length*5*side)+32768,round(p[1]+dx/length*5*side)+32768])
   rows=query(requests);levels=[float(level)];bad=False
   for k in range(1,len(pts)-1):
    center,left,right=rows[k*3:k*3+3]
    cap=min(max(r['height'],r['water']) if r['lake']==1 else r['height'] for r in [center,left,right])-1
    w=float(level) if k<=3 else max(goalwater,min(levels[-1],cap));levels.append(w)
    if center['height']-w>8 or w-center['height']>1 or levels[-2]-w>8:bad=True
   levels.append(goalwater)
   if levels[-2]-goalwater>8 or bad:
    rejected.append(dict(lake=index,reason='incision or bank constraint',goal=goal,maxCut=max(rows[k*3]['height']-levels[k] for k in range(1,len(pts)-1)),maxAbove=max(levels[k]-rows[k*3]['height'] for k in range(1,len(pts)-1)),maxStep=max(a-b for a,b in zip(levels,levels[1:])),endStep=levels[-2]-goalwater,points=pts,levels=levels));continue
   segments=[[*a,*b,aw,bw,2.2,2.2] for a,b,aw,bw in zip(pts,pts[1:],levels,levels[1:])]
   # A constant lake level through the first wet-to-dry edge prevents a notch
   # disconnected from its parent lake, while the following reach descends.
   if levels[1]<level-1:continue
   j['rivers']['segments'].extend(segments)
   # Verify finished native banks before admitting a connection. A coarse
   # receiver vertex may sit above the actual blended river surface nearby.
   candidate=out/'outlet-candidate.json';candidate.write_text(json.dumps(j,separators=(',',':')))
   samples=set()
   for seg in segments:
    count=max(1,math.ceil(math.dist(seg[:2],seg[2:4])*2))
    for k in range(count+1):
     x=round(seg[0]+(seg[2]-seg[0])*k/count);z=round(seg[1]+(seg[3]-seg[1])*k/count)
     samples.update((x+dx,z+dz) for dx in range(-5,6) for dz in range(-5,6))
   samples=sorted(samples)
   result=subprocess.run([str(exe),str(candidate)],input=''.join(json.dumps([x+32768,z+32768])+'\n' for x,z in samples),text=True,capture_output=True,check=True)
   actual=dict(zip(samples,map(json.loads,result.stdout.splitlines())))
   exposed=0
   for (x,z),r in actual.items():
    if not r['river']:continue
    for dx,dz in [(1,0),(-1,0),(0,1),(0,-1)]:
     bank=actual.get((x+dx,z+dz))
     if bank and bank['water']<=bank['height'] and r['water']>bank['height']+1:exposed+=1
   if exposed:
    del j['rivers']['segments'][-len(segments):]
    rejected.append(dict(lake=index,reason='native exposed bank',faces=exposed));continue
   j['rivers']['routes'].append(dict(source=pts[0],join=goal,receiver='river',sink=-1,nodes=len(pts),origin='lake-outlet',lakeIndex=index))
   outlets.append(dict(lakeIndex=index,lakeCenter=lake['center'],level=level,source=pts[0],join=goal,receiverLevel=goalwater,segments=segments))
   success=True;break
  if success:print('lake outlet',index,level,'->',goalwater,flush=True)
 j['rivers']['outlets']=outlets
 (out/'lake-outlets.json').write_text(json.dumps(dict(outlets=outlets,rejected=rejected),indent=2))
 print('through lakes',len(outlets),flush=True)
