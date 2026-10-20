"""Move unsafe lake approaches onto natural, retaining ground before freezing.

The receiver water level is an immutable lower bound. Lowering bank-fitted
water and then clamping it back to that bound is not a valid bank fit. This
bounded offline pass moves ordinary intermediate anchors instead; it never
adds an embankment, changes a lake level, or breaks graph connections.
"""
from pathlib import Path
import json,math,subprocess
P=Path(__file__).resolve().parents[1]

def fit_lake_banks(network,positions,upstream,radius,base,out):
    exe=P/'build/windows/x64/release/native-worldgen-check.exe'
    floors={}
    for i in network:
        dest=i
        while network[dest]['parent']>=0:dest=network[dest]['parent']
        floors[i]=network[dest]['water']
    # Sources, receiving endpoints and confluences are shared fixed anchors.
    keys=[i for i,v in network.items() if not v.get('source',False) and floors[i]>31 and v['parent']>=0 and len(upstream[i])==1]
    cache={}
    def samples(points):
        missing=sorted({(round(x),round(z)) for x,z in points}-cache.keys())
        if not missing:return
        proc=subprocess.run([str(exe),str(base)],input=''.join(json.dumps([x+32768,z+32768])+'\n' for x,z in missing),text=True,capture_output=True,check=True)
        rows=[json.loads(s) for s in proc.stdout.splitlines()];assert len(rows)==len(missing)
        cache.update(zip(missing,rows))
    def footprint(i,position):
        # Include diagonal banks and maximum runtime width modulation. The
        # extra margin also covers rounded coordinates and between-node bends.
        r=radius(network[i])*1.44+1.5;x,z=position
        return [(round(x),round(z))]+[(round(x+r*math.cos(k*math.pi/4)),round(z+r*math.sin(k*math.pi/4))) for k in range(8)]
    def cap(i,position):
        rows=[cache[p] for p in footprint(i,position)];level=floors[i]
        # Flooded cells of the actual receiver are connected water, not holes
        # in its bank. All other terrain must contain the full water block.
        bank=min(r['water']+1 if r['lake']==1 and r['water']>=level else r['height'] for r in rows)
        valid=all(r['stone']==0 and r['volcanic']==0 and r['giant']<.01 for r in rows)
        return bank,valid,rows[0]['height']
    samples([p for i in keys for p in footprint(i,positions[i])])
    bad=[i for i in keys if cap(i,positions[i])[0]<floors[i]+1 and cap(i,positions[i])[1]]
    candidates={}
    for i in bad:
        x,z=positions[i];n=network[i]['parent'];xx,zz=positions[n];dx,dz=xx-x,zz-z;length=math.hypot(dx,dz)
        # Lateral offsets preserve the longitudinal profile and avoid making
        # adjacent vertices overtake each other in the downstream direction.
        candidates[i]=[(x-dz/length*d,z+dx/length*d) for d in [-12,-8,-4,4,8,12]] if length>1e-6 else []
    samples([p for i,options in candidates.items() for pos in options for p in footprint(i,pos)])
    changed=[];unresolved=[]
    for i in bad:
        old=positions[i];parent=positions[network[i]['parent']];up=positions[upstream[i][0]];options=[]
        for pos in candidates[i]:
            bank,valid,height=cap(i,pos)
            if not valid or bank<floors[i]+1 or height-floors[i]>12:continue
            if math.dist(pos,parent)>39 or math.dist(pos,up)>39:continue
            # Prefer minimal deviation, then a smoother connection to the
            # neighbours; none of these scores relax the containment rule.
            score=math.dist(pos,old)+.15*(math.dist(pos,parent)+math.dist(pos,up))
            options.append((score,pos))
        if not options:
            unresolved.append(dict(point=list(old),receiverLevel=floors[i],bankCap=cap(i,old)[0]));continue
        _,pos=min(options);positions[i]=pos
        changed.append(dict(before=list(old),after=list(pos),receiverLevel=floors[i],bankBefore=cap(i,old)[0],bankAfter=cap(i,pos)[0]))
    # This is a bounded fit, not a claim of globally solved drainage. Keep
    # unsupported anchors in the audit for native corridor validation.
    report=dict(checkedAnchors=len(keys),movedAnchors=len(changed),changes=changed,unresolved=unresolved,maxOffset=12,addedRuntimeWork=False)
    (out/'lake-bank-fit.json').write_text(json.dumps(report,indent=2),'utf-8')
    print(json.dumps({'lakeBankAnchorsMoved':len(changed),'unresolvedAnchors':len(unresolved)}),flush=True)
