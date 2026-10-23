"""Assemble the local preview's self-contained seed worker; no system Python required."""
import argparse, ast, hashlib, json, shutil, sys, time, subprocess
from pathlib import Path
P=Path(__file__).resolve().parents[2]

def build(destination, python_home, node, template, native_tool):
    if destination.exists(): raise ValueError('Use a new staging destination')
    destination.mkdir(parents=True)
    def copy(src, rel):
        dst=destination/rel; dst.parent.mkdir(parents=True,exist_ok=True); shutil.copy2(src,dst)
    for name in ['python.exe','python3.dll','python313.dll','vcruntime140.dll','vcruntime140_1.dll','LICENSE.txt']:
        copy(python_home/name, 'python/'+name)
    shutil.copytree(python_home/'DLLs',destination/'python/DLLs')
    shutil.copytree(python_home/'Lib',destination/'python/Lib',ignore=shutil.ignore_patterns(
        'site-packages','__pycache__','test','tests','idlelib','tkinter','ensurepip'))
    import numpy
    numpy_site=Path(numpy.__file__).resolve().parent.parent
    for pattern in ['numpy','numpy.libs','numpy-*.dist-info']:
        for src in numpy_site.glob(pattern):
            shutil.copytree(src,destination/'python/Lib/site-packages'/src.name,
                            ignore=shutil.ignore_patterns('__pycache__','tests'))
    copy(node,'node/node.exe')
    # Ship Node's license alongside its runtime when provided by the distribution.
    for name in ['LICENSE','LICENSE.txt']:
        if (node.parent/name).exists(): copy(node.parent/name,'node/'+name)
    # Resolve only seed-generation dependencies. Never ship server deployment,
    # admin, or other private development scripts in a public runtime bundle.
    pending=['build_streaming_seed','build_seed','select_spawn','level_dat','bake_natural_lakes','bake_rivers','bake_scarps']
    selected=set()
    while pending:
        name=pending.pop()
        src=P/'tools'/(name+'.py')
        if not src.is_file(): src=P/'tests'/(name+'.py')
        if name in selected or not src.is_file(): continue
        selected.add(name)
        for entry in ast.walk(ast.parse(src.read_text('utf-8'))):
            if isinstance(entry,ast.ImportFrom) and entry.module: pending.append(entry.module.split('.')[0])
            elif isinstance(entry,ast.Import): pending.extend(a.name.split('.')[0] for a in entry.names)
        copy(src,str(src.relative_to(P)))
    copy(P/'tests/verify_water.py','tests/verify_water.py')
    for src in (P/'tools/seed').glob('*'):
        if src.is_file(): copy(src,'tools/seed/'+src.name)
    for rel in ['data/seed-template.json','data/runtime-template.json','src/NativeRuntime.h','src/WorldBounds.h',
                'native/vanilla-ore-rules.json','native/DEEPSLATE-LICENSE.txt','native/EPIC-TERRAIN-LICENSE.txt']:
        copy(P/rel,rel)
    copy(native_tool,'build/windows/x64/release/native-worldgen-check.exe')
    for src in sorted((P/'data/build-height-pack').rglob('*')):
        if src.is_file(): copy(src,str(src.relative_to(P)))
    for name in ['prepare_world.py','seed_service.py']:
        copy(P/'ports/windows-client'/name,'ports/windows-client/'+name)
    copy(template,'void-level.dat')
    # Import the uncached path with the shipped interpreter, not the developer's
    # environment. A successful cached-world test does not exercise these tools.
    probe="import sys,importlib;from pathlib import Path;p=Path.cwd();sys.path[:0]=[str(p/'tools'),str(p/'tests')];" \
          +"[importlib.import_module(n) for n in "+repr(sorted(selected))+"];from world_bounds import HALF;assert HALF==20000;print('uncached dependency imports passed')"
    subprocess.run([str((destination/'python/python.exe').resolve()),'-I','-X','utf8','-c',probe],
                   cwd=destination,check=True)
    hashes={str(f.relative_to(destination)).replace('\\','/'):hashlib.sha256(f.read_bytes()).hexdigest()
            for f in sorted(destination.rglob('*')) if f.is_file()}
    identity=hashlib.sha256(json.dumps(hashes,sort_keys=True).encode()).hexdigest()
    (destination/'bundle.json').write_text(json.dumps(dict(identity=identity,files=hashes),indent=2),'utf-8')
    return identity

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True)
    p.add_argument('--python-home',type=Path,required=True);p.add_argument('--node',type=Path,required=True)
    p.add_argument('--template',type=Path,required=True)
    p.add_argument('--native-tool',type=Path,required=True);a=p.parse_args()
    print(build(a.output,a.python_home,a.node,a.template,a.native_tool))
