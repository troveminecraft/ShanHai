"""Create an immutable regional recipe, without surveying the whole world."""
import argparse,json,subprocess,sys
from pathlib import Path
from copy import deepcopy
from seed_progress import ProgressReporter
from process_guard import run_checked
P=Path(__file__).resolve().parents[1]

def height_settings(data):
    data["config"].update(seaLevel=100,maxY=511,generatedMaxYExclusive=512,mountainPeakY=480,elevationRevision=2,landscapeRevision=3)
    data["config"]["stoneForest"]["valleyY"]=110

def finish_recipe(data):
    height_settings(data)
    data["config"]["badlandsRegions"]=3
    data["config"]["riverBankSurvey"]=1
    data["config"]["mountainSnowline"]=1
    data["config"]["mountainEcology"]=1
    data["config"]["regionalPlateaus"]=1
    data["config"].update(landscapeRevision=3,volcanoRevision=4,surfaceMoss=2,villageSpacing=16,lakeBasinsVersion=1)
    data["config"]["cave"].update(raisedCaveHeight=1,highMountainUnderground=1,uplandNetworks=1)
    data["config"]["smallPoolsVersion"]=3
    data['regionalGeneration']=dict(algorithm='regional-drainage-v3',coreBlocks=2048,haloBlocks=512,
                                    samplePitch=64,worldBounds=None,boundsMode='infinite',persistentPlans=True)
    data['rivers']=dict(algorithm='regional-drainage-v3',cutBanks=10,naturalBankSlope=1,sectionProfiles=1,
                        treeSubstrates=True,mainstems=[],segments=[],routes=[])
    data['seedManifest'].update(ready=True,riversReady=True,generationMode='regional-on-demand',
                                completeSeedBake=False)

def build(seed,output,status=None,native_tool=None):
    if str(int(seed))!=seed or not -(1<<63)<=int(seed)<(1<<63):raise ValueError('Signed 64-bit seed required')
    output=Path(output);output.parent.mkdir(parents=True,exist_ok=True)
    progress=ProgressReporter(status);progress.stage(0,.65,'正在建立种子与区域生成规则')
    run_checked(['node',str(P/'tools/seed/build.mjs'),seed,str(P/'data/seed-template.json'),str(output)],timeout=120,stage="seed recipe")
    data=json.loads(output.read_bytes());height_settings(data);candidates=data['seedManifest'].pop('giantCandidates')
    # Only sparse seed landmarks are resolved here, preserving the existing
    # special-forest selection policy. No desert, lake or river survey runs.
    sample=deepcopy(data);sample['config']['regionalTerrain']['enabled']=False
    scratch=output.with_suffix('.landmark.json');scratch.write_text(json.dumps(sample,separators=(',',':')),'utf-8')
    offsets=[(0,0),(-320,0),(320,0),(0,-320),(0,320),(-224,-224),(224,-224),(-224,224),(224,224)]
    queries=[[x+dx+32768,z+dz+32768] for x,z in candidates for dx,dz in offsets]
    result=run_checked([str(native_tool or P/'build/windows/x64/release/native-worldgen-check.exe'),str(scratch)],
                          input=''.join(json.dumps(q)+'\n' for q in queries),capture_output=True,timeout=300,stage="landmark sampling")
    rows=[json.loads(r) for r in result.stdout.splitlines()];chosen=None
    for i,center in enumerate(candidates):
        group=rows[i*9:i*9+9]
        usable=lambda r:108<=r['height']<=300 and not r['lake'] and not r['stone'] and not r['volcanic']
        if usable(group[0]) and sum(map(usable,group))>=7:chosen=center;break
    data['seedManifest']['forestHabitatFound']=chosen is not None
    chosen=chosen or candidates[0];data['config']['giantForest'].update(centerX=chosen[0],centerZ=chosen[1])
    finish_recipe(data)
    output.write_text(json.dumps(data,separators=(',',':')),'utf-8');scratch.unlink()
    progress.update('区域生成规则已准备',1.)
    return dict(seed=seed,generationMode='regional-on-demand',wholeWorldSamples=0)

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--seed',required=True);p.add_argument('--output',required=True,type=Path);p.add_argument('--status',type=Path);a=p.parse_args()
    print(json.dumps(build(a.seed,a.output,a.status)))
