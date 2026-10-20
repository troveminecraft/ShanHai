"""Bounded, offline main-river width/water fitting; no server-time solver."""
from pathlib import Path
import json,math,subprocess,collections,heapq
P=Path(__file__).resolve().parents[1]

def smooth(a,b,v):
    t=max(0.,min(1.,(v-a)/(b-a)));return t*t*(3-2*t)

def fit_mainstems(network,positions,upstream,radius,base,out):
    keys=[i for i,v in network.items() if v.get('main') and v['parent']>=0]
    cache={};candidates={};floors={}
    def footprint(i,r):
        x,z=positions[i];d=r*1.44+2
        return [(round(x),round(z))]+[(round(x+d*math.cos(t*math.pi/4)),round(z+d*math.sin(t*math.pi/4))) for t in range(8)]
    for i in keys:
        v=network[i];dest=i
        while network[dest]['parent']>=0:dest=network[dest]['parent']
        floors[i]=network[dest]['water'];nxt=network[v['parent']]
        slope=(v['water']-nxt['water'])/max(1,math.dist(positions[i],positions[v['parent']]))
        # Catchment area, moisture and reach slope set width independently of
        # absolute Y. Steep headwaters stay narrow, then grow into the valley.
        wide=max(12.,min(26.,math.sqrt(v['catchment'])*.040*(.7+.6*v['humidity'])))
        weight=smooth(80,320,v['age'])*(1-.8*smooth(.05,.30,slope))
        wanted=radius(v)+(wide-radius(v))*weight
        candidates[i]=[wanted*f+radius(v)*(1-f) for f in [1,.8,.6,.4,.2,0]]
    pts=sorted({p for i,rs in candidates.items() for r in rs for p in footprint(i,r)})
    exe=P/'build/windows/x64/release/native-worldgen-check.exe'
    proc=subprocess.run([str(exe),str(base)],input=''.join(json.dumps([x+32768,z+32768])+'\n' for x,z in pts),text=True,capture_output=True,check=True)
    cache=dict(zip(pts,map(json.loads,proc.stdout.splitlines())));assert len(cache)==len(pts)
    widths={};records=[]
    for i in keys:
        v=network[i];old=v['water'];chosen=radius(v)
        for r in candidates[i]:
            rows=[cache[p] for p in footprint(i,r)]
            if any(s['stone'] or s['volcanic'] or s['giant']>.01 for s in rows):continue
            cap=min(s['water']+1 if s['water']>s['height'] and s['water']==floors[i] else s['height'] for s in rows)-1
            water=max(floors[i],min(old,cap))
            if cap<floors[i] or rows[0]['height']-water>17 or old-water>8:continue
            chosen=r;v['water']=water;break
        widths[id(v)]=chosen
        records.append(dict(point=positions[i],radius=chosen,oldWater=old,water=v['water'],catchment=v['catchment']))
    # Lipschitz width transitions use graph distance rather than cell index.
    # Narrow banks constrain nearby reaches; never expand a fitted footprint.
    mainkeys=[i for i,v in network.items() if v.get('main')]
    for i in mainkeys:widths.setdefault(id(network[i]),radius(network[i]))
    heap=[(widths[id(network[i])],i) for i in mainkeys];heapq.heapify(heap)
    while heap:
        w,i=heapq.heappop(heap)
        if w!=widths[id(network[i])]:continue
        for k in upstream[i]+[network[i]['parent']]:
            if k<0 or not network[k].get('main'):continue
            limit=w+math.dist(positions[i],positions[k])*.065
            if limit<widths[id(network[k])]:widths[id(network[k])]=limit;heapq.heappush(heap,(limit,k))
    for r,i in zip(records,keys):r['radius']=widths[id(network[i])]
    (out/'mainstem-bank-fit.json').write_text(json.dumps(records,indent=2),'utf-8')
    print(json.dumps(dict(mainstemAnchors=len(keys),wideAnchors=sum(r['radius']>=12 for r in records))),flush=True)
    return widths

def describe_mainstems(network,positions,upstream,sources,radius):
    result=[]
    for source in sources:
        path=[];i=source
        while i>=0:path.append(i);i=network[i]['parent']
        points=[positions[i] for i in path];stations=[0.]
        for a,b in zip(points,points[1:]):stations.append(stations[-1]+math.dist(a,b))
        reaches=[]
        for k,i in enumerate(path[:-1]):
            a,b=points[k:k+2];left=k;right=k+1
            while left>0 and stations[k]-stations[left]<64:left-=1
            while right<len(points)-1 and stations[right]-stations[k]<64:right+=1
            u=(a[0]-points[left][0],a[1]-points[left][1]);v=(points[right][0]-a[0],points[right][1]-a[1])
            cross=u[0]*v[1]-u[1]*v[0];dot=u[0]*v[0]+u[1]*v[1]
            bend=max(-1.,min(1.,math.atan2(cross,dot)*radius(network[i])*4/max(32,stations[right]-stations[left]))) if math.hypot(*u)>0 else 0.
            reaches.append(dict(a=a,b=b,bankfullRadius=radius(network[i]),bend=bend,catchment=network[i].get('catchment',0)))
        result.append(dict(source=points[0],outlet=points[-1],length=stations[-1],reaches=reaches))
    return result

def fit_mainstem_mouths(j,base,out):
    """Resolve coast-cliff profiles at block scale before the final river bake.

    Coarse linear interpolation can put falling water above the real cliff
    face. Keep shared upstream/marine endpoints, fit only the last 64 blocks,
    and retain receiving bed checks in the normal native pipeline.
    """
    segments=j['rivers']['segments'];selected={};records=[]
    for m in j['rivers']['mainstems']:
        distance=0
        for r in reversed(m['reaches']):
            key=(*r['a'],*r['b']);distance+=math.dist(r['a'],r['b'])
            if distance>80:break
            if any(tuple(s[:4])==key and s[5]<=40 for s in segments):selected[key]=r
    queries=[];groups=[]
    for k,s in enumerate(segments):
        if tuple(s[:4]) not in selected:continue
        dx,dz=s[2]-s[0],s[3]-s[1];length=math.hypot(dx,dz);n=math.ceil(length/3);points=[]
        for i in range(n+1):
            t=i/n;x=s[0]+dx*t;z=s[1]+dz*t;r=s[6]+(s[7]-s[6])*t
            points.append([x,z,s[4]+(s[5]-s[4])*t,r])
            for side in [-1,0,1]:
                for along in [-1,0,1]:queries.append([round(x-dz/length*(r*1.44+1)*side+dx/length*along)+32768,round(z+dx/length*(r*1.44+1)*side+dz/length*along)+32768])
        groups.append((k,s,points))
    exe=P/'build/windows/x64/release/native-worldgen-check.exe'
    proc=subprocess.run([str(exe),str(base)],input=''.join(json.dumps(q)+'\n' for q in queries),text=True,capture_output=True,check=True)
    rows=list(map(json.loads,proc.stdout.splitlines()));offset=0;new=[];old={k for k,_,_ in groups};rewritten={}
    for k,s,pts in groups:
        previous=s[4]
        for i,p in enumerate(pts):
            rr=rows[offset:offset+9];offset+=9
            cap=min(32 if q['water']==31 and q['height']<31 else q['height'] for q in rr)-1
            if i not in [0,len(pts)-1]:p[2]=max(s[5],min(p[2],cap,previous))
            previous=p[2]
        # Endpoint cannot be higher than a fitted point; the lower receiving
        # level is a hard bound above, so this does not create an uphill step.
        pieces=[]
        for a,b in zip(pts,pts[1:]):pieces.append([*a[:2],*b[:2],a[2],b[2],a[3],b[3]])
        new.extend(pieces);rewritten[tuple(s[:4])]=pieces;records.append(dict(before=s,after=pieces))
    j['rivers']['segments']=[s for k,s in enumerate(segments) if k not in old]+new
    for m in j['rivers']['mainstems']:
        reaches=[]
        for r in m['reaches']:
            pieces=rewritten.get((*r['a'],*r['b']))
            if pieces:
                reaches.extend(dict(r,a=s[:2],b=s[2:4],bankfullRadius=s[6]) for s in pieces)
            else:reaches.append(r)
        m['reaches']=reaches
    (out/'mainstem-mouth-fit.json').write_text(json.dumps(records,indent=2),'utf-8')

def normalize_flow(j,out):
    """Orient flat detour edges toward a real outlet, preserving their shape.

    Merged tree detours can describe the same level channel in both
    directions. A strictly decreasing (water, outlet distance) potential
    removes these directed cycles without cutting any physical waterway.
    """
    segments=j['rivers']['segments'];reverse=collections.defaultdict(list);outgoing=collections.defaultdict(list);nodes=set()
    for s in segments:
        a,b=tuple(s[:2]),tuple(s[2:4]);reverse[b].append(s);outgoing[a].append(s);nodes.update([a,b])
    distance={p:0. for p in nodes-set(outgoing)};heap=[(0.,p) for p in distance];heapq.heapify(heap)
    while heap:
        cost,b=heapq.heappop(heap)
        if distance[b]!=cost:continue
        for s in reverse[b]:
            a=tuple(s[:2]);v=cost+math.dist(a,b)
            if v<distance.get(a,math.inf):distance[a]=v;heapq.heappush(heap,(v,a))
    assert len(distance)==len(nodes),'Drainage component lacks receiver'
    flipped=0;unique={}
    for s in segments:
        a,b=tuple(s[:2]),tuple(s[2:4])
        if s[4]==s[5] and (distance[a],a)<(distance[b],b):
            s=[*b,*a,s[5],s[4],s[7],s[6]];flipped+=1
        key=tuple(s[:4])
        if key in unique:
            old=unique[key];old[6]=max(old[6],s[6]);old[7]=max(old[7],s[7])
        else:unique[key]=s
    j['rivers']['segments']=list(unique.values())
    pending=collections.Counter();links=collections.defaultdict(list);allnodes=set()
    for s in j['rivers']['segments']:
        a,b=tuple(s[:2]),tuple(s[2:4]);pending[b]+=1;links[a].append(b);allnodes.update([a,b])
    queue=collections.deque(p for p in allnodes if not pending[p]);visited=0
    while queue:
        a=queue.popleft();visited+=1
        for b in links[a]:
            pending[b]-=1
            if not pending[b]:queue.append(b)
    assert visited==len(allnodes),'Drainage graph contains cycle'
    (out/'flow-dag.json').write_text(json.dumps(dict(nodes=visited,acyclic=True,reorientedFlatEdges=flipped,physicalChannelsRetained=True),indent=2),'utf-8')
