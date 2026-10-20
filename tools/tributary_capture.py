"""Resolve early tributary contacts after the final mainstem width is known.

Offline only: a tributary that reaches a lower main channel must terminate
there instead of crossing it and emerging again as an elevated side channel.
Keep shared branches, headwaters, receiver levels and mainstem shapes intact.
"""
from pathlib import Path
import collections, json, math, subprocess

P=Path(__file__).resolve().parents[1]

def _capture_pass(j, base, out, skips):
    rivers=j['rivers'];records=[];rejected=[]
    fitted=rivers.get('confluenceProfiles',0)>=1
    adaptive=rivers.get('capturePolicy',0)>=2
    continuous=rivers.get('capturePolicy',0)>=3
    safety=base
    if adaptive:
        safety=out/'contact-surface.json';safety.write_text(json.dumps(j,separators=(',',':')),'utf-8')
    def key(a,b):return (tuple(a),tuple(b))
    for route in rivers['routes']:
        if route['receiver']!='river' or (skips and tuple(route['source']) not in skips):continue
        segments=rivers['segments'];outgoing=collections.defaultdict(list);incoming=collections.Counter()
        for s in segments:outgoing[tuple(s[:2])].append(s);incoming[tuple(s[2:4])]+=1
        main={key(r['a'],r['b']):r for m in rivers['mainstems'] for r in m['reaches']}
        main.update({(b,a):r for (a,b),r in list(main.items())})
        path=[];p=tuple(route['source']);join=tuple(route['join']);seen=set()
        while p!=join and p not in seen and len(outgoing[p])==1:
            seen.add(p);s=outgoing[p][0]
            if key(s[:2],s[2:4]) in main:break
            path.append(s);p=tuple(s[2:4])
        if p!=join or len(path)<5:continue
        # No pruning of another route's source or shared confluence.
        protected={tuple(r[k]) for r in rivers['routes'] for k in ['source','join']}
        wide=[s for s in segments if key(s[:2],s[2:4]) in main and min(s[6:])>=8
              and s[4]-s[5]<=2 and s[5]>36]
        wideBins=collections.defaultdict(list)
        halo=max((max(s[6:]) for s in path),default=4)+2
        for m in wide:
            reach=max(m[6:])+halo
            for gz in range(math.floor((min(m[1],m[3])-reach)/64),math.floor((max(m[1],m[3])+reach)/64)+1):
                for gx in range(math.floor((min(m[0],m[2])-reach)/64),math.floor((max(m[0],m[2])+reach)/64)+1):wideBins[gx,gz].append(m)
        proposal=None
        for index,s in enumerate(path[:-2]):
            if index<max(2,skips.get(tuple(route['source']),2)):continue
            if any(incoming[tuple(t[:2])]>1 or tuple(t[:2]) in protected for t in path[index+1:]):continue
            length=math.dist(s[:2],s[2:4]);steps=max(1,math.ceil(length/3))
            for step in range(steps+1):
                t=step/steps;x=s[0]+(s[2]-s[0])*t;z=s[1]+(s[3]-s[1])*t
                level=s[4]+(s[5]-s[4])*t;radius=s[6]+(s[7]-s[6])*t
                options=[]
                for m in wideBins[math.floor(x/64),math.floor(z/64)]:
                    dx,dz=m[2]-m[0],m[3]-m[1];v=max(0.,min(1.,((x-m[0])*dx+(z-m[1])*dz)/(dx*dx+dz*dz)))
                    if v<1e-8:v=0.
                    elif v>1-1e-8:v=1.
                    q=(m[0]+v*dx,m[1]+v*dz);water=m[4]+v*(m[5]-m[4]);r=m[6]+v*(m[7]-m[6])
                    if level<water or math.dist((x,z),q)>r+radius+2:continue
                    if fitted and level-water>12:continue
                    # First near-contact must shorten a real, unshared tail.
                    if sum(math.dist(a[:2],a[2:4]) for a in path[index:])<(40 if fitted else 80):continue
                    if math.dist(s[:2],q)>38 or math.dist(s[:2],q)<2:continue
                    # Reject a receiver whose downstream path returns to this
                    # tributary (or does not reach this route's original join).
                    downstream=tuple(m[2:4]);visited=set()
                    while downstream!=join and downstream not in visited and len(outgoing[downstream])==1:
                        visited.add(downstream);downstream=tuple(outgoing[downstream][0][2:4])
                    if downstream!=join or any(tuple(a[:2]) in visited for a in path):continue
                    options.append((math.dist((x,z),q),m,v,q,water,r))
                if options:
                    proposal=(index,min(options,key=lambda o:o[0]));break
            if proposal:break
        if not proposal:continue
        index,(_,receiver,receiverT,q,water,receiverRadius)=proposal
        # Grade the last approach upstream from the real receiver. This only
        # lowers levels, never makes water climb, and is limited to 128 blocks.
        kept=[list(s) for s in path[:index]]
        start=path[index][:2];kept.append([*start,*q,path[index][4],water,path[index][6],path[index][6]])
        if adaptive:
            # Fit the last 128 blocks to the receiving channel's actual banks.
            # A fixed slope/flat reach either perches the water or cuts a deep
            # artificial gorge when the approach crosses a steep valley side.
            lengths=[math.dist(t[:2],t[2:4]) for t in kept];total=sum(lengths)
            nodes=[];travel=0.
            for t,length in zip(kept,lengths):
                n=max(1,math.ceil(length/2)) if travel+length>total-128 else 1
                for k in range(n):
                    u=k/n;nodes.append([t[0]+u*(t[2]-t[0]),t[1]+u*(t[3]-t[1]),t[4]+u*(t[5]-t[4]),t[6]+u*(t[7]-t[6]),travel+length*u])
                travel+=length
            nodes.append([*q,water,kept[-1][7],total])
            queries=[];indices=[]
            for k,n in enumerate(nodes[:-1]):
                if n[4]<total-128:continue
                directions=[(nodes[k+1][0]-n[0],nodes[k+1][1]-n[1])]
                if continuous and k:directions.append((n[0]-nodes[k-1][0],n[1]-nodes[k-1][1]))
                for dx,dz in directions:
                    length=math.hypot(dx,dz)
                    for side in [-1,1]:
                        queries.append([round(n[0]-dz/length*(n[3]*1.4+1)*side)+32768,round(n[1]+dx/length*(n[3]*1.4+1)*side)+32768]);indices.append(k)
            proc=subprocess.run([str(P/'build/windows/x64/release/native-worldgen-check.exe'),str(safety)],input=''.join(json.dumps(t)+'\n' for t in queries),text=True,capture_output=True,check=True)
            rows=[json.loads(t) for t in proc.stdout.splitlines()];assert len(rows)==len(indices)
            oldLevels=[n[2] for n in nodes]
            for k,r in zip(indices,rows):
                support=r['water'] if r['river']>0 else r['height']-(1 if continuous else 0)
                nodes[k][2]=max(water,min(nodes[k][2],support))
            for k in range(1,len(nodes)):nodes[k][2]=min(nodes[k][2],nodes[k-1][2])
            # Allow the short steep descent already present on the valley wall,
            # while bounding each step and never changing the upstream source.
            grade=1.5
            for k in range(len(nodes)-2,-1,-1):
                nodes[k][2]=min(nodes[k][2],nodes[k+1][2]+grade*math.dist(nodes[k][:2],nodes[k+1][:2]))
            gradedDistance=max((total-n[4] for n,w in zip(nodes,oldLevels) if n[2]<w-1e-6),default=0.)
            if gradedDistance>128+1e-6 or nodes[0][2]!=path[0][4]:
                rejected.append(dict(source=route['source'],proposalIndex=index,reason='bank fitting exceeds 128 blocks or changes headwater'));continue
            kept=[[*a[:2],*b[:2],a[2],b[2],a[3],b[3]] for a,b in zip(nodes,nodes[1:])]
        else:
            # Finish the descent at the receiving wet edge, before its centre.
            # The old centre-only grade leaves an elevated tongue inside the main.
            lastLength=math.dist(kept[-1][:2],kept[-1][2:4])
            flatReach=min(lastLength-1.,receiverRadius*1.15+kept[-1][6]*1.2+2.) if fitted else 0.
            distance=-flatReach;nextLevel=water;tooLong=False;grade=.20 if fitted else .13
            gradedDistance=0.
            for k in range(len(kept)-1,-1,-1):
                s=kept[k];old=path[k]
                downstreamDistance=distance
                distance+=math.dist(s[:2],s[2:4]);s[5]=nextLevel
                s[4]=max(nextLevel,min(s[4],water+grade*distance));nextLevel=s[4]
                if s[4]==old[4]:
                    # Once the upstream level is unchanged, every earlier reach
                    # already joins it. Do not require an extra unchanged segment.
                    # Split where the old profile first meets the new grade: the
                    # modification budget measures that exact point, not the
                    # enclosing polyline node (which could overshoot by 40 blocks).
                    if k<len(kept)-1 and s[5]<old[5]:
                        da=old[4]-(water+grade*distance)
                        db=old[5]-(water+grade*downstreamDistance)
                        if da<=0<db:
                            u=max(0.,min(1.,-da/(db-da)))
                            mid=[old[0]+u*(old[2]-old[0]),old[1]+u*(old[3]-old[1])]
                            level=old[4]+u*(old[5]-old[4]);radius=old[6]+u*(old[7]-old[6])
                            gradedDistance=distance-u*math.dist(old[:2],old[2:4])
                            pieces=[]
                            if u>1e-8:pieces.append([*old[:2],*mid,old[4],level,old[6],radius])
                            if u<1-1e-8:pieces.append([*mid,*old[2:4],level,s[5],radius,old[7]])
                            kept[k:k+1]=pieces
                        else:gradedDistance=max(0.,distance)
                    else:gradedDistance=max(0.,downstreamDistance)
                    tooLong=gradedDistance>128+1e-6
                    break
                if distance>128:tooLong=True;break
                gradedDistance=max(0.,distance)
            if tooLong or kept[0][4]!=path[0][4]:
                rejected.append(dict(source=route['source'],proposalIndex=index,reason='grade exceeds 128 blocks or changes headwater'));continue
            shared={tuple(s[:2]):s[4] for s in path if incoming[tuple(s[:2])]>1 or tuple(s[:2]) in protected}
            if any(tuple(s[:2]) in shared and s[4]!=shared[tuple(s[:2])] for s in kept):
                rejected.append(dict(source=route['source'],proposalIndex=index,reason='grade would alter a shared upstream water level'));continue
            if fitted and flatReach>0:
                remaining=flatReach
                for k in range(len(kept)-1,-1,-1):
                    s=kept[k];length=math.dist(s[:2],s[2:4])
                    if remaining>=length:
                        s[4]=s[5]=water;remaining-=length;continue
                    u=(length-remaining)/length
                    rim=[s[0]+u*(s[2]-s[0]),s[1]+u*(s[3]-s[1])];rr=s[6]+u*(s[7]-s[6])
                    kept[k:k+1]=[[*s[:2],*rim,s[4],water,s[6],rr],[*rim,*s[2:4],water,water,rr,s[7]]]
                    break
        shared={tuple(t[:2]):t[4] for t in path if incoming[tuple(t[:2])]>1 or tuple(t[:2]) in protected}
        if any(tuple(t[:2]) in shared and t[4]!=shared[tuple(t[:2])] for t in kept):
            rejected.append(dict(source=route['source'],proposalIndex=index,reason='grade would alter a shared upstream water level'));continue
        def unchanged(s):
            # Splitting an old reach does not make its unchanged upstream bank
            # a new shortcut. Compare geometry/levels, not shifted list indices.
            for old in path:
                dx,dz=old[2]-old[0],old[3]-old[1];den=dx*dx+dz*dz
                ts=[((s[i]-old[0])*dx+(s[i+1]-old[1])*dz)/den for i in (0,2)]
                if not (-1e-7<=ts[0]<=ts[1]<=1+1e-7):continue
                if all(math.hypot(s[i]-old[0]-t*dx,s[i+1]-old[1]-t*dz)<1e-6 and abs(s[4+i//2]-(old[4]+t*(old[5]-old[4])))<1e-6 for i,t in zip((0,2),ts)):return True
            return False
        changed=[s for s in kept if not unchanged(s)] if continuous else [s for k,s in enumerate(kept) if k>=len(path) or s!=path[k]]
        queries=[]
        for s in changed:
            dx,dz=s[2]-s[0],s[3]-s[1];length=math.hypot(dx,dz);n=max(1,math.ceil(length/3))
            for k in range(n+1):
                u=k/n;x=s[0]+u*dx;z=s[1]+u*dz;w=s[4]+u*(s[5]-s[4]);r=s[6]+u*(s[7]-s[6])
                for side in [-1,0,1]:queries.append((round(x-dz/length*(r*1.4+1)*side),round(z+dx/length*(r*1.4+1)*side),w))
        proc=subprocess.run([str(P/'build/windows/x64/release/native-worldgen-check.exe'),str(safety)],
            input=''.join(json.dumps([x+32768,z+32768])+'\n' for x,z,_ in queries),text=True,capture_output=True,check=True)
        rows=[json.loads(s) for s in proc.stdout.splitlines()];assert len(rows)==len(queries)
        def unsafe(w,r):
            if r['stone'] or r['volcanic'] or r['giant']>.01:return True
            # Existing bank tops share the water block's Y. Field water is
            # quantized; compare block levels, not a fractional route level.
            if adaptive and r['river']>0 and math.floor(w)<=r['water']+1:return False
            return r['height']<math.floor(w)+(0 if adaptive else 1) or r['height']-w>(24 if adaptive else 18)
        bad=[dict(x=x,z=z,target=w,height=r['height'],water=r['water'],river=r['river'],giant=r['giant'])
             for (x,z,w),r in zip(queries,rows) if unsafe(w,r)]
        if bad:
            rejected.append(dict(source=route['source'],proposalIndex=index,reason='shortcut lacks safe natural banks',grade=grade,examples=bad));continue
        removed={id(s) for s in path};removed.add(id(receiver))
        pieces=[]
        if receiverT>1e-8:pieces.append([*receiver[:2],*q,receiver[4],water,receiver[6],receiverRadius])
        if receiverT<1-1e-8:pieces.append([*q,*receiver[2:4],water,receiver[5],receiverRadius,receiver[7]])
        rivers['segments']=[s for s in segments if id(s) not in removed]+kept+pieces
        for m in rivers['mainstems']:
            reaches=[]
            for r in m['reaches']:
                if key(r['a'],r['b'])==key(receiver[:2],receiver[2:4]):
                    reaches.extend(dict(r,a=s[:2],b=s[2:4],bankfullRadius=s[6]) for s in pieces)
                elif key(r['b'],r['a'])==key(receiver[:2],receiver[2:4]):
                    reaches.extend(dict(r,a=s[2:4],b=s[:2],bankfullRadius=s[7]) for s in reversed(pieces))
                else:reaches.append(r)
            m['reaches']=reaches
        oldJoin=route['join'];route['join']=list(q);route['nodes']=len(kept)+1
        records.append(dict(source=route['source'],oldJoin=oldJoin,join=list(q),water=water,
                            gradedDistance=gradedDistance,gradeLimit=128,
                            oldSegments=len(path),newSegments=len(kept),removedTailBlocks=sum(math.dist(s[:2],s[2:4]) for s in path)-sum(math.dist(s[:2],s[2:4]) for s in kept)))
    rivers['earlyConfluences']=4 if continuous else 3 if adaptive else 2
    result=dict(captured=len(records),changes=records,rejected=rejected,runtimeAlgorithmUnchanged=True,offlineBankFit=adaptive)
    return result

def capture_tributaries(j, base, out):
    from headwater_runs import bake_headwater_runs
    changes=[];unresolved={};skips={};attempts=[]
    continuous=j['rivers'].get('capturePolicy',0)>=3
    for attempt in range(6 if continuous else 1):
        bake_headwater_runs(j)
        result=_capture_pass(j,base,out,skips);attempts.append(result)
        for r in result['changes']:
            changes.append(r);unresolved.pop(tuple(r['source']),None)
        for r in result['rejected']:unresolved[tuple(r['source'])]=r
        if not result['rejected']:break
        # An unsafe first contact is not a reason to keep the old perched tail:
        # search the next contact, with the same bank/cut/headwater safeguards.
        skips={tuple(r['source']):r['proposalIndex']+1 for r in result['rejected']}
    bake_headwater_runs(j)
    result=dict(captured=len(changes),changes=changes,rejected=list(unresolved.values()),attempts=len(attempts),runtimeAlgorithmUnchanged=True,offlineBankFit=j['rivers'].get('capturePolicy',0)>=2)
    (out/'tributary-capture.json').write_text(json.dumps(result,indent=2),'utf-8')
    (out/'tributary-attempts.json').write_text(json.dumps(attempts,indent=2),'utf-8')
    print(json.dumps(result),flush=True);return result
