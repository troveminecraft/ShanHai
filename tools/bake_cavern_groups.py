"""Freeze sparse terrain-height controls; no chunks, caves or voxel maps baked."""
from pathlib import Path
import hashlib,json,sys
P=Path(__file__).resolve().parents[1]
from native_chunk_io import run
def identity(data):return hashlib.sha256(json.dumps({k:v for k,v in data.items() if k not in ('lavaFlows','iceRegions','version','cavernPlan','stoneForestGrotto')},sort_keys=True,separators=(',',':')).encode()).hexdigest()
def bake(source):
 source=Path(source);data=json.loads(source.read_text('utf-8'))
 if data['config']['cave'].get('algorithm')!='bounded-cavern-groups-v3':return
 plan=run(P/'build/windows/x64/release/native-worldgen-check.exe',source,[{'bakeCavernPlan':True}])[0]
 plan['sourceSha256']=identity(data);data['cavernPlan']=plan
 source.write_text(json.dumps(data,separators=(',',':')),'utf-8')
 print(json.dumps(dict(cavernHeightControls=len(plan['samples']),seed=plan['seed'])),flush=True)
if __name__=='__main__':bake(sys.argv[1])
