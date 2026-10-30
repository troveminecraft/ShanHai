"""Reproducible offline recipes; does not create or enter any game world."""
import argparse,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from process_guard import run_checked
from build_streaming_seed import finish_recipe

def prepare(output):
 output.mkdir(parents=True,exist_ok=True)
 for name,seed in [('positive','5'),('negative','-1'),('wide','9223372036854775807')]:
  path=output/(name+'.json')
  run_checked(['node',str(ROOT/'tools/seed/build.mjs'),seed,str(ROOT/'data/seed-template.json'),str(path)],
              timeout=120,stage='offline fixture')
  data=json.loads(path.read_bytes());data['seedManifest'].pop('giantCandidates')
  # Use seeded landmark defaults: this fixture tests the actual kernel, not spawn selection.
  valley=data['config']['stoneForest']['valleyY']
  cliffs=dict(data['config']['cliffs'])
  finish_recipe(data)
  # Preserve legacy fixtures; expanded recipes have separate height/water tests.
  data['config'].update(seaLevel=31,maxY=319)
  data['config'].pop('generatedMaxYExclusive',None)
  data['config'].pop('mountainPeakY',None)
  data['config'].pop('badlandsRegions',None)
  data['config'].pop('smallPoolsVersion',None)
  data['config'].pop('riverBankSurvey',None)
  data['config'].pop('coastalContinuity',None)
  for key in ('glacierAltitudeRevision','glacierHaloBlocks','uplandWaterways'):data['config'].pop(key,None)
  data['config']['cliffs']=cliffs
  data['config'].pop('mountainEcology',None)
  data['config'].pop('regionalPlateaus',None)
  for key in ('landscapeRevision','elevationRevision','volcanoRevision','surfaceMoss','villageSpacing','lakeBasinsVersion'):data['config'].pop(key,None)
  data['config']['stoneForest']['valleyY']=valley
  data['config']['cave'].pop('raisedCaveHeight',None)
  data['config']['cave'].pop('highMountainUnderground',None)
  data['config']['cave'].pop('uplandNetworks',None)
  data['regionalGeneration']['algorithm']='regional-drainage-v2'
  data['rivers']['algorithm']='regional-drainage-v2'
  path.write_text(json.dumps(data,separators=(',',':')),'utf-8')
if __name__=='__main__':
 parser=argparse.ArgumentParser();parser.add_argument('--output',required=True,type=Path)
 prepare(parser.parse_args().output)
