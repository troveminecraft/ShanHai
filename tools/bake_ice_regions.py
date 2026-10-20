"""Freeze climate-screened region anchors; never search for them in BDS."""
from pathlib import Path
import json,hashlib,subprocess
P=Path(__file__).resolve().parents[1]
def identity(j):return hashlib.sha256(json.dumps({k:v for k,v in j.items() if k not in ('lavaFlows','version','iceRegions')},sort_keys=True,separators=(',',':')).encode()).hexdigest()
def bake(source):
 source=Path(source);j=json.loads(source.read_text('utf-8'));j.pop('iceRegions',None)
 source.write_text(json.dumps(j,separators=(',',':')),'utf-8')
 if not j['config'].get('snowUpdate',{}).get('enabled'):return
 r=subprocess.run([str(P/'build/windows/x64/release/native-worldgen-check.exe'),str(source)],input=json.dumps({'iceRegions':True})+'\n',text=True,capture_output=True,check=True)
 j['iceRegions']={'algorithm':'glacier-regions-v1','seed':j['config']['seed'],'regions':json.loads(r.stdout),'sourceSha256':identity(j)}
 source.write_text(json.dumps(j,separators=(',',':')),'utf-8');print('Frozen ice regions:',len(j['iceRegions']['regions']),flush=True)
def stamp(source):
 source=Path(source);j=json.loads(source.read_text('utf-8'))
 if 'iceRegions' in j:j['iceRegions']['sourceSha256']=identity(j);source.write_text(json.dumps(j,separators=(',',':')),'utf-8')
if __name__=='__main__':
 import sys
 bake(sys.argv[1])
