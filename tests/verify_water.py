"""Release regression: closed lake banks, unchanged basin heights, chunk determinism inputs."""
from pathlib import Path
import json, subprocess, struct, statistics
import numpy as np

P=Path(__file__).resolve().parents[1]
import sys
sys.path.insert(0,str(P/'tools'))
from native_chunk_io import decode,run
O=P/'outputs/agent-a-0.6.14-r01'
EXE=P/'build/windows/x64/release/native-worldgen-check.exe'
def main():
    source=P/'data/native-input.json';data=json.loads(source.read_text('utf-8'))
    details=json.loads((O/'lake-bake.json').read_text('utf-8'))['lakes'];wet_total=0;border_total=0;points=set()
    for lake,d in zip(data['naturalLakes'],details):
        terrain=json.loads((O/f"basin-{d['index']}.json").read_text('utf-8'))
        heights={(int(r[0]),int(r[1])):int(r[2]) for r in terrain}
        alcoves={(int(r[0]),int(r[1])):int(r[8]) for r in terrain}
        wet={(x,z) for z,x0,x1,t in lake['rows'] if t==1 for x in range(x0,x1+1)}
        assert len(wet)==lake['area'];wet_total+=len(wet)
        for x,z in wet:
            assert heights[x,z]<lake['level'] and lake['level']-heights[x,z]<=14
            for dx,dz in [(1,0),(-1,0),(0,1),(0,-1)]:
                k=x+dx,z+dz
                if k not in wet:
                    assert heights[k]>=lake['level'] and alcoves[k]==-32768; border_total+=1;points.add(k)
        points.add(tuple(lake['center']))
    points=sorted(points)
    req=[[x+32768,z+32768] for x,z in points]
    current=run(EXE,source,req)
    baseline=run(O/'baseline-native.exe',O/'baseline-input.json',req)
    assert all(a['height']==b['height'] for a,b in zip(current,baseline)), 'Lake excavation detected'
    coast=[(2688,-3104),(2720,-3088),(2736,-3104)]
    new=run(EXE,source,[[x+32768,z+32768] for x,z in coast])
    old=run(O/'baseline-native.exe',O/'baseline-input.json',[[x+32768,z+32768] for x,z in coast])
    assert all(a['height']>b['height'] and a['rock'] for a,b in zip(new,old))
    coarse=json.loads((O/'coarse-surface.json').read_text('utf-8'))
    beach=next((int(r[0]),int(r[1])) for r in coarse if 32<=r[2]<=34 and r[4]==0 and r[6]==0 and abs(r[0])<3800 and abs(r[1])<3800)
    control=run(EXE,source,[[beach[0]+32768,beach[1]+32768]])[0]
    prev=run(O/'baseline-native.exe',O/'baseline-input.json',[[beach[0]+32768,beach[1]+32768]])[0]
    assert control['height']==prev['height'] and control['coast']==0
    coords=[(x//16,z//16) for x,z in coast+[tuple(a['center']) for a in data['naturalLakes']]+[beach]]
    coords+= [(i*37%256-128,i*71%256-128) for i in range(42)]
    timings={};counts={};decoded={}
    for label,exe,inp in [('old',O/'baseline-native.exe',O/'baseline-input.json'),('new',EXE,source)]:
        requests=[dict(cx=x,cz=z,output=str(O/f'{label}-{i}.bin')) for i,(x,z) in enumerate(coords)]
        timing=run(exe,inp,requests);timings[label]={'medianMs':statistics.median(a['ms'] for a in timing),'totalMs':sum(a['ms'] for a in timing),'maxMs':max(a['ms'] for a in timing)}
        chunks=[decode(O/f'{label}-{i}.bin') for i in range(len(coords))];decoded[label]=chunks
        counts[label]=sum(int((a[:,:,4:16]==8).sum()) for a in chunks)
    assert counts['new']<counts['old']*.5 and counts['new']>0
    # Feature-disabled compatibility proves the new code preserves the old kernel.
    run(EXE,O/'baseline-input.json',[dict(cx=x,cz=z,output=str(O/f'compat-{i}.bin')) for i,(x,z) in enumerate(coords)])
    for i in range(len(coords)):
        assert (O/f'compat-{i}.bin').read_bytes()==(O/f'old-{i}.bin').read_bytes(), f'Baseline compatibility {coords[i]}'
    places=[('山地海崖',2688,-3104),('平缓海岸对照',*beach),('自然高山湖',-604,2053),('高海拔冰湖',-556,-2758),('北部山间湖',863,-3816)]
    spots=[]
    for name,x,z in places:
        i=coords.index((x//16,z//16));col=decoded['new'][i][z%16,x%16];top=int(np.flatnonzero(col)[-1])-64;y=min(310,top+12)
        assert col[y+64]==0 and col[y+65]==0
        spots.append(dict(name=name,x=x,y=y,z=z,verifiedAir=True))
    report=dict(lakes=len(details),waterColumns=wet_total,closedBorderChecks=border_total,unchangedLakeHeightSamples=len(points),coast=[dict(x=x,z=z,old=b['height'],new=a['height']) for (x,z),a,b in zip(coast,new,old)],beach=beach,chunks=len(coords),deepWaterCounts=counts,timings=timings,spots=spots)
    (O/'water-validation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(report,ensure_ascii=True))
if __name__=='__main__':main()
