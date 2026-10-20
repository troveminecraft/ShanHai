"""Build a complete immutable seed package; never write any BDS world or chunks."""
from pathlib import Path
import argparse,json,subprocess,os,tempfile,hashlib,sys
from seed_progress import ProgressReporter
P=Path(__file__).resolve().parents[1]
if __name__=='__main__':
 a=argparse.ArgumentParser();a.add_argument('--seed',required=True);a.add_argument('--output',type=Path,required=True);a.add_argument('--diagnostics',type=Path,required=True);a.add_argument('--status',type=Path);a=a.parse_args()
 progress=ProgressReporter(a.status)
 def phase(text,fraction=None):progress.update(text,fraction)
 def child_progress(start,end):
  return ['--status',str(a.status.resolve()),'--progress-start',str(start),'--progress-end',str(end)] if a.status else []
 progress.stage(0.,.005,'正在建立种子与地形噪声')
 seed=int(a.seed);assert str(seed)==a.seed and -(1<<63)<=seed<(1<<63),'Signed 64-bit decimal seed required'
 output=a.output.resolve();output.parent.mkdir(parents=True,exist_ok=True);diag=a.diagnostics.resolve();diag.mkdir(parents=True,exist_ok=True)
 # Staging prevents a failed basin analysis from replacing the active package.
 with tempfile.TemporaryDirectory(prefix='karma-seed-',dir=diag) as temp:
  source=Path(temp)/'native-input.json'
  subprocess.run(['node',str(P/'tools/seed/build.mjs'),a.seed,str(P/'data/seed-template.json'),str(source)],check=True)
  # A seed-selected forest must land on usable ground, not in an ocean.
  j=json.loads(source.read_text('utf-8'));candidates=j['seedManifest'].pop('giantCandidates')
  offsets=[(0,0),(-320,0),(320,0),(0,-320),(0,320),(-224,-224),(224,-224),(-224,224),(224,224)]
  # Choose the special forest against the stable pre-regional ground.
  # Its footprint is protected by regionalHeight, so a terrain distribution
  # update does not move this established seed-derived landmark.
  from copy import deepcopy
  sampling=deepcopy(j)
  if 'regionalTerrain' in sampling['config']:sampling['config']['regionalTerrain']['enabled']=False
  source.write_text(json.dumps(sampling,separators=(',',':')),encoding='utf-8')
  query=[[x+dx+32768,z+dz+32768] for x,z in candidates for dx,dz in offsets]
  sample=subprocess.run([str(P/'build/windows/x64/release/native-worldgen-check.exe'),str(source)],input=''.join(json.dumps(q)+'\n' for q in query),text=True,capture_output=True,check=True)
  rows=[json.loads(line) for line in sample.stdout.splitlines()];chosen=None
  for i,center in enumerate(candidates):
   group=rows[i*9:i*9+9]
   usable=lambda r:47<=r['height']<=200 and not r['lake'] and r['stone']==0 and r['volcanic']==0
   if usable(group[0]) and sum(usable(r) for r in group)>=7:chosen=center;break
  if chosen is None:
   # A valid all-ocean seed may lack this biome: retain a derived candidate;
   # height eligibility suppresses trees instead of manufacturing a flat island.
   chosen=candidates[0];j['seedManifest']['forestHabitatFound']=False
  else:j['seedManifest']['forestHabitatFound']=True
  j['config']['giantForest'].update(centerX=chosen[0],centerZ=chosen[1])
  source.write_text(json.dumps(j,separators=(',',':')),encoding='utf-8')
  print(json.dumps({'forestCenter':chosen,'habitatFound':j['seedManifest']['forestHabitatFound']}),flush=True)
  from bake_deserts import bake as bake_deserts
  progress.stage(.005,.07,'正在分析沙漠与气候区域')
  bake_deserts(source,diag,phase)
  progress.stage(.07,.45,'正在分析湖泊盆地')
  subprocess.run([sys.executable,str(P/'tools/bake_natural_lakes.py'),'--source',str(source),'--output',str(diag)]+child_progress(.07,.45),check=True)
  progress.stage(.45,.90,'正在计算江河水系与岸线')
  subprocess.run([sys.executable,str(P/'tools/bake_rivers.py'),'--source',str(source),'--coarse',str(diag/'coarse-surface.json'),'--output',str(diag)]+child_progress(.45,.90),check=True)
  progress.stage(.90,.93,'正在分析悬崖地形')
  subprocess.run([sys.executable,str(P/'tools/bake_scarps.py'),'--source',str(source),'--output',str(diag)],check=True)
  from arid_landforms import bake as bake_arid
  progress.stage(.93,.95,'正在准备干旱地形')
  bake_arid(source,diag/'arid')
  from bake_ice_regions import bake as bake_ice, stamp as stamp_ice
  progress.stage(.95,.965,'正在准备冰川')
  bake_ice(source)
  from bake_cavern_groups import bake as bake_caverns
  progress.stage(.965,.975,'正在准备地下洞穴')
  bake_caverns(source)
  from bake_stone_grotto import bake as bake_grotto
  progress.stage(.975,.983,'正在准备石峰林水洞')
  bake_grotto(source,diag/'stone-grotto')
  from bake_lava_flows import bake as bake_lava
  progress.stage(.983,.99,'正在准备火山熔岩')
  bake_lava(source,diag/'lava')
  progress.stage(.99,.992,'正在完成冰川数据')
  stamp_ice(source)
  progress.stage(.992,.995,'正在校验完整种子数据')
  # Hillside entrance carving postponed by user; retain natural cave slopes.
  j=json.loads(source.read_text('utf-8'));assert j['seedManifest']['terrainSeed']==a.seed and j['seedManifest']['ready']
  identity={k:v for k,v in j.items() if k not in ['lavaFlows','iceRegions','stoneForestGrotto','cavernPlan','version','naturalLakes','naturalLakeSource','rivers','riverSource','overhangCliffs','aridLandforms']}
  assert j['naturalLakeSource']==hashlib.sha256(json.dumps(identity,sort_keys=True,separators=(',',':')).encode()).hexdigest()
  import re
  j['version']=re.search(r'KernelVersion\[\]="([^"]+)"',(P/'src/NativeRuntime.h').read_text('utf-8')).group(1)
  source.write_text(json.dumps(j,separators=(',',':')),'utf-8')
  staging=output.with_name(output.name+'.seed-tmp')
  with staging.open('xb') as f:f.write(source.read_bytes());f.flush();os.fsync(f.fileno())
  os.replace(staging,output)
  from world_bounds import MIN,MAX,CHUNKS
  output.with_suffix('.extent.json').write_text(json.dumps(dict(sourceSha256=hashlib.sha256(output.read_bytes()).hexdigest(),min=MIN,max=MAX,chunksPerSide=CHUNKS,completeSeedBake=True),indent=2),'utf-8')
  progress.update('地形数据准备完成',1.)
 print(json.dumps({'terrainSeed':a.seed,'lakes':len(j['naturalLakes']),'output':str(output),'complete':True}))
