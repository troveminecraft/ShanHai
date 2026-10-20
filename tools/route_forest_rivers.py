"""Route forest reaches around immutable pre-river giant trunk footprints."""
from pathlib import Path
import json,math,heapq,subprocess,collections,bisect
import numpy as np
P=Path(__file__).resolve().parents[1]

def route_forest(j,base,out):
 exe=P/'build/windows/x64/release/native-worldgen-check.exe';file=out/'forest-sites.json'
 query=dict(forest=True,footprintsOnly=True,output=str(file))
 subprocess.run([str(exe),str(base)],input=json.dumps(query)+'\n',text=True,capture_output=True,check=True)
 trees=json.loads(file.read_text());segments=j['rivers']['segments']
 if not trees or not segments:return
 guards=[[t['id'],t['x'],t['z'],t['radius']] for t in trees]
 loX=min(t['x'] for t in trees)-40;loZ=min(t['z'] for t in trees)-40
 nx=max(t['x'] for t in trees)-loX+41;nz=max(t['z'] for t in trees)-loZ+41
 clear=np.full((nz,nx),50.,dtype=np.float32)
 for t in trees:
  x,z=t['x']-loX,t['z']-loZ;r=t['radius'];n=math.ceil(r+18)
  xa,xb=max(0,x-n),min(nx,x+n+1);za,zb=max(0,z-n),min(nz,z+n+1)
  zz,xx=np.mgrid[za:zb,xa:xb];clear[za:zb,xa:xb]=np.minimum(clear[za:zb,xa:xb],np.hypot(xx-x,zz-z)-r)
 def clearance(x,z):
  gx,gz=round(x)-loX,round(z)-loZ
  return float(clear[gz,gx]) if 0<=gx<nx and 0<=gz<nz else 50.
 bins=collections.defaultdict(list)
 for tree in trees:bins[tree['x']//32,tree['z']//32].append(tree)
 def edgeClear(a,b):
  dx,dz=b[0]-a[0],b[1]-a[1];length=dx*dx+dz*dz;value=50.
  for gz in range(math.floor((min(a[1],b[1])-18)/32),math.floor((max(a[1],b[1])+18)/32)+1):
   for gx in range(math.floor((min(a[0],b[0])-18)/32),math.floor((max(a[0],b[0])+18)/32)+1):
    for t in bins[gx,gz]:
     u=max(0.,min(1.,((t['x']-a[0])*dx+(t['z']-a[1])*dz)/max(1e-9,length)))
     value=min(value,math.hypot(t['x']-a[0]-u*dx,t['z']-a[1]-u*dz)-t['radius'])
  return value
 mapped={}
 for s in segments:
  for v in [tuple(s[:2]),tuple(s[2:4])]:
   if v in mapped:continue
   if clearance(*v)>2.4:mapped[v]=v;continue
   x,z=map(round,v);candidates=[(math.dist(v,(x+dx,z+dz)),x+dx,z+dz) for dz in range(-24,25) for dx in range(-24,25) if clearance(x+dx,z+dz)>=2.4]
   if not candidates:raise RuntimeError('No safe giant-forest channel anchor')
   _,x,z=min(candidates);mapped[v]=(x,z)
 def path(a,b,aw,bw):
  # Sample natural relief once per corridor, offline. Bilinear heights avoid
  # putting millions of terrain evaluations in the graph search/runtime.
  x0,x1=math.floor((min(a[0],b[0])-48)/4)*4,math.ceil((max(a[0],b[0])+48)/4)*4
  z0,z1=math.floor((min(a[1],b[1])-48)/4)*4,math.ceil((max(a[1],b[1])+48)/4)*4
  coords=[(x,z) for z in range(z0,z1+1,4) for x in range(x0,x1+1,4)]
  sample=subprocess.run([str(exe),str(base)],input=''.join(json.dumps([x+32768,z+32768])+'\n' for x,z in coords),text=True,capture_output=True,check=True)
  heights={c:r['height'] for c,r in zip(coords,map(json.loads,sample.stdout.splitlines()))}
  def terrainCost(v):
   x,z=v;xx=min(x1-4,math.floor(x/4)*4);zz=min(z1-4,math.floor(z/4)*4);fx=(x-xx)/4;fz=(z-zz)/4
   h=(heights[xx,zz]*(1-fx)+heights[xx+4,zz]*fx)*(1-fz)+(heights[xx,zz+4]*(1-fx)+heights[xx+4,zz+4]*fx)*fz
   t=max(0.,min(1.,((x-a[0])*(b[0]-a[0])+(z-a[1])*(b[1]-a[1]))/max(1.,math.dist(a,b)**2)))
   w=aw+(bw-aw)*t
   return .35*max(0.,h-w-3)**2+2.*max(0.,w+1-h)**2
  if edgeClear(a,b)>=6 and all(terrainCost((a[0]+(b[0]-a[0])*t/16,a[1]+(b[1]-a[1])*t/16))<1 for t in range(17)):
   n=max(1,math.ceil(math.dist(a,b)/32));return [a]+[(a[0]+(b[0]-a[0])*t/n,a[1]+(b[1]-a[1])*t/n) for t in range(1,n)]+[b]
  start=tuple(map(round,a));goal=tuple(map(round,b));heap=[(2.5*math.dist(start,goal),0.,start)];cost={start:0.};prev={};visited=0;closed=set()
  while heap:
   _,g,u=heapq.heappop(heap)
   if g!=cost[u] or u in closed:continue
   closed.add(u)
   if u==goal:break
   visited+=1
   if visited>160000:raise RuntimeError('Forest river routing budget exceeded '+str((a,b)))
   for dx,dz in [(1,0),(-1,0),(0,1),(0,-1),(1,1),(1,-1),(-1,1),(-1,-1)]:
    v=u[0]+dx,u[1]+dz
    if v in closed:continue
    if not(x0<=v[0]<=x1 and z0<=v[1]<=z1) or clearance(*v)<2.4:continue
    if dx and dz and min(clearance(u[0]+dx,u[1]),clearance(u[0],u[1]+dz))<2.4:continue
    # Prefer wider gaps; a weighted heuristic bounds long forest-run searches.
    ng=g+math.hypot(dx,dz)*(1+5./max(1,clearance(*v))**2+terrainCost(v))
    if ng<cost.get(v,math.inf):cost[v]=ng;prev[v]=u;heapq.heappush(heap,(ng+2.5*math.dist(v,goal),ng,v))
  else:raise RuntimeError('No continuous path between forest river anchors '+str((a,b)))
  points=[goal]
  while points[-1]!=start:points.append(prev[points[-1]])
  points.reverse();points=[a]+points+[b];result=[points[0]];i=0
  while i<len(points)-1:
   k=i+1
   while k+1<len(points) and math.dist(points[i],points[k+1])<=8 and edgeClear(points[i],points[k+1])>=2.15 and terrainCost(((points[i][0]+points[k+1][0])/2,(points[i][1]+points[k+1][1])/2))<=max(terrainCost(points[i]),terrainCost(points[k+1]))+.5:k+=1
   if points[k]!=result[-1]:result.append(points[k])
   i=k
  return result
 # Resolve a continuous obstacle run as one route. Keeping every blocked
 # coarse anchor forced adjacent reaches to circle opposite sides of a tree.
 incoming=collections.defaultdict(list);byStart={tuple(s[:2]):i for i,s in enumerate(segments)}
 for i,s in enumerate(segments):incoming[tuple(s[2:4])].append(i)
 anchors={tuple(r[k]) for r in j['rivers']['routes'] for k in ['source','join']}
 affected=set()
 for i,s in enumerate(segments):
  a,b=tuple(s[:2]),tuple(s[2:4])
  if mapped[a]!=a or mapped[b]!=b or edgeClear(a,b)<max(s[6],s[7])*1.12+1.1:affected.add(i)
 # Include a short approach/departure so the detour rejoins in its downstream
 # direction instead of returning to a compulsory point behind the obstacle.
 for i in list(affected):
  s=segments[i];a,b=tuple(s[:2]),tuple(s[2:4])
  if len(incoming[a])==1 and a not in anchors:affected.add(incoming[a][0])
  if b in byStart and len(incoming[b])==1 and b not in anchors:affected.add(byStart[b])
 groups=[];seen=set()
 for i,s in enumerate(segments):
  if i not in affected or i in seen:continue
  head=i
  while True:
   a=tuple(segments[head][:2]);prev=incoming[a]
   if a in anchors or len(prev)!=1 or prev[0] not in affected:break
   head=prev[0]
  chain=[];k=head
  while k not in seen:
   seen.add(k);chain.append(k);end=tuple(segments[k][2:4]);nxt=byStart.get(end)
   if end in anchors or len(incoming[end])!=1 or nxt not in affected:break
   k=nxt
  if chain:groups.append(chain)
 routed=[s.copy() for i,s in enumerate(segments) if i not in affected];changed=0;refit=set();fixed=set()
 for group in groups:
  first,last=segments[group[0]],segments[group[-1]]
  a,b=mapped[tuple(first[:2])],mapped[tuple(last[2:4])]
  if a==b:continue
  fixed.update([a,b])
  pts=path(a,b,first[4],last[5]);total=sum(math.dist(u,v) for u,v in zip(pts,pts[1:]));distance=0;changed+=len(group)
  stations=[0.]
  for idx in group:stations.append(stations[-1]+math.dist(segments[idx][:2],segments[idx][2:4]))
  def profile(t):
   distance=t*stations[-1];k=min(len(group)-1,bisect.bisect_right(stations,distance)-1);q=segments[group[k]];f=(distance-stations[k])/max(1e-9,stations[k+1]-stations[k]);return q[4]+(q[5]-q[4])*f,q[6]+(q[7]-q[6])*f
  for u,v in zip(pts,pts[1:]):
   length=math.dist(u,v);t0,t1=distance/total,(distance+length)/total;distance+=length
   aw,ar=profile(t0);bw,br=profile(t1);radius=min(ar,br,max(1.2,(edgeClear(u,v)-1.1)/1.12))
   routed.append([*u,*v,aw,bw,radius,radius]);refit.update([tuple(u),tuple(v)])
 # Coincident detours are actual confluences, not duplicated superposed streams.
 levels={};outgoing=collections.defaultdict(list)
 for s in routed:
  a,b=tuple(s[:2]),tuple(s[2:4]);levels[a]=min(levels.get(a,math.inf),s[4]);levels[b]=min(levels.get(b,math.inf),s[5]);outgoing[a].append(b)
 # Rerouting changes the banks. Refit against the uncut new corridor before
 # propagating water downstream, otherwise a safe tree detour can float above
 # the downhill bank. Receiving lake/sea levels remain fixed.
 reverse=collections.defaultdict(list)
 for a,children in outgoing.items():
  for b in children:reverse[b].append(a)
 sinks=set(levels)-set(outgoing);floor={p:31. for p in levels};queue=collections.deque(sinks)
 for p in sinks:floor[p]=levels[p]
 while queue:
  b=queue.popleft()
  for a in reverse[b]:
   if floor[a]<floor[b]:floor[a]=floor[b];queue.append(a)
 # Internal detour levels may recover up to four blocks of the old incision.
 # Group endpoints stay fixed so unaffected branches cannot acquire a step.
 for node in refit-fixed-sinks:levels[node]+=4.
 keys=[];queries=[]
 for seg in routed:
  dx,dz=seg[2]-seg[0],seg[3]-seg[1];length=math.hypot(dx,dz)
  for node,r in [(tuple(seg[:2]),seg[6]),(tuple(seg[2:4]),seg[7])]:
   if node not in refit or node in sinks:continue
   for side in [-1,1]:
    for along in [-2,0,2]:
     keys.append(node);queries.append([round(node[0]-dz/length*(max(1.2,r)+2.5)*side+dx/length*along)+32768,round(node[1]+dx/length*(max(1.2,r)+2.5)*side+dz/length*along)+32768])
 response=subprocess.run([str(exe),str(base)],input=''.join(json.dumps(q)+'\n' for q in queries),text=True,capture_output=True,check=True)
 for node,row in zip(keys,map(json.loads,response.stdout.splitlines())):levels[node]=max(floor[node],min(levels[node],row['height']-1))
 queue=collections.deque(levels)
 while queue:
  a=queue.popleft()
  for b in outgoing[a]:
   if levels[b]>levels[a]:levels[b]=levels[a];queue.append(b)
 unique={}
 for s in routed:
  a,b=tuple(s[:2]),tuple(s[2:4]);s[4],s[5]=max(31.,levels[a]),max(31.,levels[b]);unique[a,b]=s
 j['rivers']['segments']=list(unique.values());j['rivers']['forestGuards']=guards
 for r in j['rivers']['routes']:r['source']=mapped[tuple(r['source'])];r['join']=mapped[tuple(r['join'])]
 for h in j['rivers'].get('headwaters',[]):
  h.setdefault('selectionSource',h['source']);h['source']=mapped[tuple(h['source'])]
 report=dict(trees=len(trees),reroutedReaches=changed,detourGroups=len(groups),segments=len(unique))
 (out/'forest-routing.json').write_text(json.dumps(report,indent=2));print(json.dumps(report),flush=True)
