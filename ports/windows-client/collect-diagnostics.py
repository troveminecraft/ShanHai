"""Collect local diagnostics only. Never uploads data or opens a player world."""
import argparse,hashlib,json,time,zipfile,os
from pathlib import Path

def collect(instance,output,system_dumps=False):
    instance=Path(instance).resolve();mod=instance/'mods/mountains&seas'
    if not (mod/'manifest.json').is_file():raise ValueError('mountains&seas is not installed in this instance')
    if output.exists():raise ValueError('Output already exists')
    files={};skipped=[]
    def add(path,name,limit=128*1024*1024):
        if path.is_file():
            if path.stat().st_size>limit:skipped.append({'file':name,'reason':'size limit'});return
            files[name]=path
    for name in ['manifest.json','client-config.json','runtime/bundle.json']:add(mod/name,'mod/'+name)
    for name in ['latest.log']:add(instance/'logs'/name,'logs/'+name)
    for pattern in ['trace_*.log','minidump_*.dmp']:
        for f in sorted((instance/'logs/crash').glob(pattern),key=lambda p:p.stat().st_mtime,reverse=True)[:2]:add(f,'crash/'+f.name)
    diagnostics=mod/'diagnostics'
    for f in diagnostics.glob('*'):
        if f.suffix in ['.json','.txt','.log']:add(f,'diagnostics/'+f.name)
    sessions=sorted((diagnostics/'sessions').glob('*'),key=lambda p:p.stat().st_mtime,reverse=True)[:3]
    for directory in sessions:
        for f in directory.glob('*'):
            if f.suffix in ['.json','.jsonl']:add(f,'diagnostics/sessions/'+directory.name+'/'+f.name)
    worlds=set()
    for path in [diagnostics/'world-session.json',*[p/'session.json' for p in sessions]]:
        try:
            j=json.loads(path.read_bytes());world=Path(j['worldPath']).resolve()
            if world.is_relative_to(instance) and (world/'karma-worldgen.json').is_file():worlds.add(world)
        except (OSError,ValueError,KeyError):pass
    for world in worlds:
        add(world/'karma-worldgen.json','worlds/'+world.name+'/karma-worldgen.json')
        for f in (world/'karma').rglob('*'):
            rel=f.relative_to(world)
            if f.suffix in ['.json','.jsonl','.txt'] and not any(x in ['cache','regional-cache'] for x in rel.parts):add(f,'worlds/'+world.name+'/'+rel.as_posix())
    if system_dumps:
        folder=Path(os.environ.get('LOCALAPPDATA',''))/'CrashDumps'
        for f in sorted(folder.glob('Minecraft.Windows.exe.*.dmp'),key=lambda p:p.stat().st_mtime,reverse=True)[:2]:add(f,'system-dumps-unassigned/'+f.name)
    output.parent.mkdir(parents=True,exist_ok=True)
    manifest={'createdUnixMs':int(time.time()*1000),'instance':str(instance),'mod':json.loads((mod/'manifest.json').read_bytes()),'dllSha256':hashlib.sha256((mod/'karma-worldgen-client.dll').read_bytes()).hexdigest(),'uploaded':False,'gameStarted':False,'systemDumpInstanceAssociation':'unverified' if system_dumps else 'not collected','files':{},'skipped':skipped}
    with zipfile.ZipFile(output,'x',zipfile.ZIP_DEFLATED) as archive:
        for name,path in sorted(files.items()):
            try:data=path.read_bytes()
            except OSError as e:skipped.append({'file':name,'reason':str(e)});continue
            archive.writestr(name,data);manifest['files'][name]={'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()}
        archive.writestr('collection.json',json.dumps(manifest,ensure_ascii=False,indent=2))
    return manifest

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--instance',type=Path,default=Path(__file__).resolve().parents[2]);p.add_argument('--output',type=Path);p.add_argument('--include-system-dumps',action='store_true');a=p.parse_args()
    output=a.output or a.instance/'mods/mountains&seas/diagnostics'/('support-'+time.strftime('%Y%m%d-%H%M%S')+'.zip')
    result=collect(a.instance,output,a.include_system_dumps);print(json.dumps({'output':str(output.resolve()),'files':len(result['files']),'skipped':len(result['skipped']),'uploaded':False},ensure_ascii=False))
