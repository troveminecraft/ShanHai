"""Offline source habitat screening shared by streams and main rivers."""
from pathlib import Path
from world_bounds import HALF, SIDE, CHUNKS
import json, subprocess, hashlib
P=Path(__file__).resolve().parents[1]
POLICY=dict(version=1,minGround=90,minHumidity=.45,snowMinGround=120,
            snowMaxTemperature=.22,snowMinHumidity=.30,minNeighbourRelief=6,
            sampleRadius=64,minCatchmentCells=12)

def sample_batches(exe,scratch,batches):
    """Keep one immutable field for all batches, draining stdout during writes."""
    from concurrent.futures import ThreadPoolExecutor
    process=subprocess.Popen([str(exe),str(scratch)],stdin=subprocess.PIPE,
                             stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
    def send(queries):
        process.stdin.write(''.join(json.dumps(q)+'\n' for q in queries));process.stdin.flush()
    try:
        with ThreadPoolExecutor(max_workers=1) as writer:
            try:
                for batch,queries in batches:
                    sending=writer.submit(send,queries)
                    rows=[]
                    for _ in queries:
                        line=process.stdout.readline()
                        if not line:raise RuntimeError('Headwater sampling failed: '+process.stderr.read())
                        rows.append(json.loads(line))
                    sending.result()
                    yield batch,rows
                process.stdin.close()
                if process.wait()!=0:raise RuntimeError('Headwater sampling failed: '+process.stderr.read())
            except BaseException:
                if process.poll() is None:process.kill()
                raise
    finally:
        if process.poll() is None:process.kill();process.wait()
        for pipe in (process.stdin,process.stdout,process.stderr):
            if not pipe.closed:pipe.close()

def select_sources(data, ids, accumulation, scratch, output, progress=None):
    ids=sorted(set(ids));n=CHUNKS
    def xy(i):return (i%n*16-HALF,i//n*16-HALF)
    offsets=[(0,0),(-64,0),(64,0),(0,-64),(0,64)]
    digest=hashlib.sha256(scratch.read_bytes()).hexdigest()
    cached=output/'headwater-candidates.json'
    if cached.exists():
        previous=json.loads(cached.read_text('utf-8'))
        records=previous.get('candidates',[])
        if previous.get('sourceSha256')==digest and previous.get('policy')==POLICY and len(records)==len(ids) and all(r['source']==list(xy(i)) and r['catchmentCells']==int(accumulation[i]) for i,r in zip(ids,records)):
            print('Reused verified headwater samples',len(ids),flush=True)
            if progress:progress(len(ids),len(ids))
            return {i:r for i,r in zip(ids,records) if r['accepted']}
    accepted={};report=[]
    # Bound diagnostic JSON memory as the world area increases. Each batch
    # uses the same immutable field and retains the same candidate ordering.
    def batches():
        for begin in range(0,len(ids),256):
            batch=ids[begin:begin+256]
            queries=[[x+dx+32768,z+dz+32768] for i in batch for x,z in [xy(i)] for dx,dz in offsets]
            yield batch,queries
    for batch,rows in sample_batches(P/'build/windows/x64/release/native-worldgen-check.exe',scratch,batches()):
        for k,i in enumerate(batch):
            record=assess(xy(i),rows[k*5:k*5+5],accumulation[i])
            report.append(record)
            if record['accepted']:accepted[i]=record
        if progress:progress(len(report),len(ids))
    (output/'headwater-candidates.json').write_text(json.dumps(dict(policy=POLICY,candidates=report,sourceSha256=digest),indent=2),encoding='utf-8')
    print(json.dumps(dict(headwaterCandidates=len(ids),eligibleHeadwaters=len(accepted))),flush=True)
    return accepted

def assess(point,group,catchment):
    s=group[0]
    humidity=sum(a['H'] for a in group)/5
    relief=max(a['height'] for a in group)-min(a['height'] for a in group)
    rain=s['H']>=POLICY['minHumidity'] and humidity>=POLICY['minHumidity']
    snow=s['height']>=POLICY['snowMinGround'] and s['T']<POLICY['snowMaxTemperature'] and s['H']>=POLICY['snowMinHumidity'] and humidity>=POLICY['snowMinHumidity']
    reason=None
    if s['height']<POLICY['minGround']:reason='lowland'
    elif s['biome'] in [7,11,12,16] or not (rain or snow):reason='dry-source'
    elif s['water']>=s['height'] or s['lake'] or s['stone']>.01 or s['volcanic']>0:reason='incompatible-ground'
    elif relief<POLICY['minNeighbourRelief']:reason='flat-catchment'
    elif catchment<POLICY['minCatchmentCells']:reason='small-catchment'
    record=dict(source=list(point),ground=s['height'],humidity=s['H'],meanHumidity=humidity,
                temperature=s['T'],biome=s['biome'],localRelief=relief,catchmentCells=int(catchment),
                sourceKind='rain-fed' if rain else 'snowmelt',accepted=reason is None,reason=reason)
    return record

def validate_final_sources(data,scratch):
    heads=data['rivers']['headwaters'];offsets=[(0,0),(-64,0),(64,0),(0,-64),(0,64)]
    queries=[[round(h['source'][0])+dx+32768,round(h['source'][1])+dz+32768] for h in heads for dx,dz in offsets]
    r=subprocess.run([str(P/'build/windows/x64/release/native-worldgen-check.exe'),str(scratch)],input=''.join(json.dumps(q)+'\n' for q in queries),text=True,capture_output=True,check=True)
    rows=[json.loads(line) for line in r.stdout.splitlines()]
    for i,h in enumerate(heads):
        result=assess(h['source'],rows[i*5:i*5+5],h['catchmentCells'])
        if not result['accepted']:raise RuntimeError('Final relocated headwater is unsuitable: '+json.dumps(result))
        h.update(result)
