"""Choose a natural dry spawn from the compiled generator, including plants."""
from pathlib import Path
import json,subprocess,struct,hashlib,tempfile
P=Path(__file__).resolve().parents[1]

class NativeQuery:
 def __init__(self,exe,source):
  self.errors=tempfile.TemporaryFile(mode='w+t',encoding='utf-8')
  self.process=subprocess.Popen([str(exe),str(source)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,
   stderr=self.errors,text=True,encoding='utf-8',bufsize=1)
 def __enter__(self):return self
 def __call__(self,requests):
  result=[]
  for request in requests:
   try:
    self.process.stdin.write(json.dumps(request)+'\n');self.process.stdin.flush()
    line=self.process.stdout.readline()
    if not line:raise BrokenPipeError('Terrain worker ended before answering')
    result.append(json.loads(line))
   except (BrokenPipeError,OSError):
    self.errors.seek(0);raise RuntimeError('Terrain worker failed: '+self.errors.read())
  return result
 def __exit__(self,*args):
  try:
   self.process.stdin.close()
   try:self.process.wait(timeout=2)
   except subprocess.TimeoutExpired:self.process.kill();self.process.wait()
  finally:self.process.stdout.close();self.errors.close()
def query(exe,source,requests):
 r=subprocess.run([str(exe),str(source)],input=''.join(json.dumps(q)+'\n' for q in requests),text=True,capture_output=True,check=True)
 return [json.loads(line) for line in r.stdout.splitlines()]
def columns(path):
 b=path.read_bytes();assert b[:4]==b'KW2\0';pos=260;out=[]
 for _ in range(256):
  n=struct.unpack_from('<H',b,pos)[0];pos+=2;col=[]
  for _ in range(n):
   length,block=struct.unpack_from('<HH',b,pos);pos+=4;col.extend([block]*length)
  assert len(col)==384;out.append(col)
 assert pos==len(b);return out
def select(source,exe,output=None):
 # One engine keeps regional plans and generated spawn chunks warm across
 # candidate checks. Previously every query rebuilt a separate engine.
 with NativeQuery(exe,source) as worker:return _select(source,exe,worker,output)

def _select(source,exe,worker,output=None):
 candidates=[]
 # Nearest usable mainland first, without a special-biome location tie.
 for radius,step in [(768,64),(3520,256)]:
  candidates=sorted({(x,z) for z in range(-radius,radius+1,step) for x in range(-radius,radius+1,step)},key=lambda p:(p[0]*p[0]+p[1]*p[1],p))
  # A broad search needs only coarse eligibility. Prepare exact region plans
  # when checking a candidate's 9x9 support, not for every distant probe.
  samples=worker([{'spawnProbe':[x+32768,z+32768]} for x,z in candidates])
  possible=[(x,z,s) for (x,z),s in zip(candidates,samples) if 34<=s['height']<=150 and s['water']<s['height'] and s['lava']<s['height'] and s['stone']==0 and s['giant']<.01 and s['volcanic']==0 and not s['lake'] and not s['river'] and not s['rock'] and s.get('wetland',0)==0]
  possible.sort(key=lambda q:(q[2]['biome'] not in [7,8,12],q[0]*q[0]+q[1]*q[1]))
  with tempfile.TemporaryDirectory(prefix='karma-spawn-',dir=source.parent) as temp:
   cache={}
   for x,z,s in possible[:120]:
    h=s['height'];points=[(x+dx,z+dz) for dz in range(-4,5) for dx in range(-4,5)]
    nearby=worker([[X+32768,Z+32768] for X,Z in points])
    if any(abs(a['height']-h)>1 or a['water']>=a['height'] or a['lava']>=a['height'] or a['rock'] or a['stone'] or a['cliffLow']!=-32768 or a['drip'] for a in nearby):continue
    chunks=sorted({(X//16,Z//16) for X,Z in points}-cache.keys())
    paths={(cx,cz):Path(temp)/f'{cx}-{cz}.bin' for cx,cz in chunks}
    worker([dict(cx=cx,cz=cz,output=str(f)) for (cx,cz),f in paths.items()])
    for key,f in paths.items():cache[key]=columns(f)
    safe=True
    for (X,Z),a in zip(points,nearby):
     col=cache[X//16,Z//16][(Z%16)*16+X%16];g=a['height']
     if col[g+64] not in [3,4,5,32,33] or any(v not in [0,36,37,38,39,40,41,62,63,64,71,72,73,74,88] for v in col[g+65:g+68]):safe=False;break
    if not safe:continue
    center=cache[x//16,z//16][(z%16)*16+x%16]
    if center[h+65]!=0 or center[h+66]!=0:continue
    report=dict(spawn=dict(x=x,y=h+1,z=z),biome=s['biome'],checkedColumns=81,policy='natural dry ground; 9x9 solid support and passable headroom; no stone-forest binding',inputSha256=hashlib.sha256(source.read_bytes()).hexdigest(),generatorSha256=hashlib.sha256(exe.read_bytes()).hexdigest())
    if output:output.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    return report
 raise RuntimeError('No verified natural spawn found; installation must not proceed')
if __name__=='__main__':
 import argparse
 parser=argparse.ArgumentParser();parser.add_argument('--source',type=Path,default=P/'data/native-input.json');parser.add_argument('--output',type=Path,default=P/'data/spawn-selection.json');args=parser.parse_args()
 print(json.dumps(select(args.source,P/'build/windows/x64/release/native-worldgen-check.exe',args.output),ensure_ascii=False))
