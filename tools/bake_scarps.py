"""Fit rare dry mountain scarps after hydrology; never scan terrain during play."""
from pathlib import Path
import argparse,json,math,sys,hashlib
P=Path(__file__).resolve().parents[1];E=P/'build/windows/x64/release/native-worldgen-check.exe'
from native_chunk_io import run
from noise_scarps import build

def bake(source,output):
    output.mkdir(parents=True,exist_ok=True)
    j=json.loads(source.read_text('utf-8'));j.pop('overhangCliffs',None);j.pop('aridLandforms',None)
    scratch=output/'scarp-base-input.json';scratch.write_text(json.dumps(j,separators=(',',':')),'utf-8')
    candidates=run(E,scratch,[dict(scarpCandidates=True)])[0];accepted=[];rejected=[]
    for c in candidates:
        x,z=c['x'],c['z'];c['back']=80.
        # Include the entire deformation domain and its displaced noise samples.
        testHalf=min(80.,c['halfLength']*1.25)
        radius=math.hypot(testHalf,80.)+30
        if any(math.hypot(x-a['x'],z-a['z'])<240 for a in accepted):continue
        water=False
        for a in j['rivers']['segments']:
            ax,az,bx,bz,aw,bw,ar,br=a;dx,dz=bx-ax,bz-az
            t=max(0,min(1,((x-ax)*dx+(z-az)*dz)/(dx*dx+dz*dz)))
            if math.hypot(x-ax-t*dx,z-az-t*dz)<radius+max(ar,br)*1.6+48:water=True;break
        if not water:
            for lake in j['naturalLakes']:
                if any(abs(zz-z)<radius+32 and a-radius-32<x<b+radius+32 for zz,a,b,_ in lake['rows']):water=True;break
        if water:rejected.append(dict(x=x,z=z,reason='water buffer'));continue
        n=math.ceil(radius/8)*8
        points=[(x+dx,z+dz) for dz in range(-n,n+1,8) for dx in range(-n,n+1,8) if dx*dx+dz*dz<=radius*radius]
        rows=run(E,scratch,[[xx+32768,zz+32768] for xx,zz in points])
        bad=any(r['height']<40 or r['water']>=r['height'] or r['lake'] or r['river'] or r['stone']>0 or r['volcanic']>0 or r['coast']>.01 or r['giant']>0 or r['wetland']>0 or r['cliffLow']>-32768 for r in rows)
        if bad:rejected.append(dict(x=x,z=z,reason='protected terrain / old alcove'));continue
        site,metrics=build(scratch,output,c,len(accepted))
        if site is None:rejected.append(dict(x=x,z=z,**metrics));continue
        accepted.append(site)
        (output/f"noise-metrics-{len(accepted)-1}.json").write_text(json.dumps(metrics,indent=2),'utf-8')
        if len(accepted)==32:break
    identity={k:v for k,v in j.items() if k not in ['lavaFlows','iceRegions','stoneForestGrotto','cavernPlan','version','overhangCliffs']}
    sourceHash=hashlib.sha256(json.dumps(identity,sort_keys=True,separators=(',',':')).encode()).hexdigest()
    j['overhangCliffs']=dict(algorithm='noise-warped-scarps-v3',seed=j['config']['seed'],sourceSha256=sourceHash,sites=accepted)
    source.write_text(json.dumps(j,separators=(',',':')),'utf-8')
    report=dict(candidates=len(candidates),accepted=len(accepted),sites=[{k:v for k,v in a.items() if k!='rows'} for a in accepted],rejected=rejected)
    (output/'scarp-plan.json').write_text(json.dumps(report,indent=2),'utf-8')
    print(json.dumps(dict(candidates=len(candidates),accepted=len(accepted))),flush=True)
    return report

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();bake(a.source.resolve(),a.output.resolve())
