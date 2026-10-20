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
    for name,expected in meta['runtimeFiles'].items():
        p=runtime/name
        if not p.resolve().is_relative_to((runtime/'native').resolve()):raise ValueError('Unsafe runtime path')
        if hashlib.sha256(p.read_bytes()).hexdigest()!=expected:raise ValueError('Runtime checksum mismatch: '+name)
    data=json.loads((runtime/'native/native-input.json').read_bytes())
    if data['version']!=marker['kernel'] or data['seedManifest']['terrainSeed']!=seed:raise ValueError('Native input mismatch')
    if not data['seedManifest'].get('ready') or not data['seedManifest'].get('riversReady'):raise ValueError('Incomplete hydrology')
    return dict(packageVerified=True,gameTested=False,seed=seed,name=tags['LevelName'][1],
                world=str(world),kernel=marker['kernel'],profileSha256=digest)

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('world',type=Path);a=p.parse_args()
    print(json.dumps(check(a.world),ensure_ascii=False,indent=2))
