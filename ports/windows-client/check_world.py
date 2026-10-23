"""Read-only preflight. This verifies package identity, not in-game integration."""
import argparse, hashlib, json, sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools'))
from level_dat import read_file

def check(world):
    world=Path(world)
    marker=json.loads((world/'karma-worldgen.json').read_bytes())
    runtime=world/'karma'
    if marker['host']!='windows-client':raise ValueError('Wrong host')
    if marker!=json.loads((runtime/'active-world.json').read_bytes()):raise ValueError('Activation mismatch')
    _,_,tags=read_file(world/'level.dat')
    seed=str(tags['RandomSeed'][1])
    if seed!=marker['worldSeed'] or seed!=marker['terrainSeed']:raise ValueError('World/terrain seed mismatch')
    if tags['Generator']!=(3,2) or 'TheVoid' not in tags['FlatWorldLayers'][1]:raise ValueError('World generator changed')
    meta=json.loads((runtime/'terrain.json').read_bytes())
    profile=(runtime/'profile.json').read_bytes()
    digest=hashlib.sha256(profile).hexdigest()
    if digest!=marker['sha256'] or digest!=meta['sha256']:raise ValueError('Profile hash mismatch')
    if json.loads(profile)!={k:v for k,v in meta.items() if k!='sha256'}:raise ValueError('Metadata mismatch')
    if meta['terrainSeed']!=seed or meta['worldSeed']!=seed:raise ValueError('Metadata seed mismatch')
    if meta['version']!=marker['kernel']:raise ValueError('Kernel mismatch')
    build_max = meta.get('buildMaxYExclusive', 320)
    generated_max = meta.get('generatedMaxYExclusive', 320)
    if type(build_max) is not int or type(generated_max) is not int or build_max not in (320, 512) or generated_max not in (320, 512) or generated_max > build_max:
        raise ValueError('Unsupported world height')
    if type(meta.get('maxY',generated_max-1)) is not int or meta.get('maxY', generated_max-1)!=generated_max-1:
        raise ValueError('Generated height metadata mismatch')
    if any(type(marker.get(key,320)) is not int for key in ('buildMaxYExclusive','generatedMaxYExclusive')) or marker.get('buildMaxYExclusive',320)!=build_max or marker.get('generatedMaxYExclusive',320)!=generated_max:
        raise ValueError('Marker height mismatch')
    if build_max == 512:
        pack = meta.get('buildHeightPack', {})
        folder = pack.get('folder','')
        if not folder or Path(folder).name!=folder or folder in ('.','..'):
            raise ValueError('Unsafe height pack path')
        pack_root = world/'behavior_packs'/folder
        references=json.loads((world/'world_behavior_packs.json').read_bytes())
        expected_ref=dict(pack_id=pack['uuid'], version=pack['version'])
        if not isinstance(references,list) or references.count(expected_ref)!=1:
            raise ValueError('Height pack selection mismatch')
        if not pack.get('files'):raise ValueError('Missing height pack checksums')
        for name,expected in pack['files'].items():
            path=pack_root/name
            if not path.resolve().is_relative_to(pack_root.resolve()):raise ValueError('Unsafe height pack file')
            if hashlib.sha256(path.read_bytes()).hexdigest()!=expected:raise ValueError('Height pack checksum mismatch')
        manifest=json.loads((pack_root/'manifest.json').read_bytes())
        if manifest['header']['uuid']!=pack['uuid'] or manifest['header']['version']!=pack['version']:
            raise ValueError('Height pack identity mismatch')
        dimension=json.loads((pack_root/'dimensions/overworld.json').read_bytes())['minecraft:dimension']
        if dimension['description']['identifier']!='minecraft:overworld' or dimension['components']['minecraft:dimension_bounds']!={'min':-64,'max':512} or dimension['components']['minecraft:generation']!={'generator_type':'void'}:
            raise ValueError('Height pack dimension mismatch')
    for name,expected in meta['runtimeFiles'].items():
        p=runtime/name
        if not p.resolve().is_relative_to((runtime/'native').resolve()):raise ValueError('Unsafe runtime path')
        if hashlib.sha256(p.read_bytes()).hexdigest()!=expected:raise ValueError('Runtime checksum mismatch: '+name)
    data=json.loads((runtime/'native/native-input.json').read_bytes())
    native_max=data.get('config',{}).get('generatedMaxYExclusive',320)
    if type(native_max) is not int or native_max!=generated_max:
        raise ValueError('Native recipe height differs from world profile')
    if data['version']!=marker['kernel'] or data['seedManifest']['terrainSeed']!=seed:raise ValueError('Native input mismatch')
    if not data['seedManifest'].get('ready') or not data['seedManifest'].get('riversReady'):raise ValueError('Incomplete hydrology')
    return dict(packageVerified=True,gameTested=False,seed=seed,name=tags['LevelName'][1],
                world=str(world),kernel=marker['kernel'],profileSha256=digest,
                buildMaxYExclusive=build_max,generatedMaxYExclusive=generated_max)

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('world',type=Path);a=p.parse_args()
    print(json.dumps(check(a.world),ensure_ascii=False,indent=2))
