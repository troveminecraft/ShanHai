"""Package the Windows client and its verified, self-contained seed runtime."""
import argparse,hashlib,json,zipfile
from pathlib import Path
P=Path(__file__).resolve().parents[1]

def package(runtime,output,binary=None):
    bundle=json.loads((runtime/'bundle.json').read_bytes())
    for name,digest in bundle['files'].items():
        source=(runtime/name).resolve()
        if not source.is_relative_to(runtime.resolve()) or hashlib.sha256(source.read_bytes()).hexdigest()!=digest:
            raise ValueError('Invalid runtime file: '+name)
    if output.exists():raise ValueError('Use a new output path')
    output.parent.mkdir(parents=True,exist_ok=True)
    with zipfile.ZipFile(output,'x',zipfile.ZIP_DEFLATED) as archive:
        root='karma-worldgen-client/'
        for name in ('karma-worldgen-client.dll','manifest.json'):
            source=P/'ports/windows-client'/('build/windows/x64/release/'+name if name.endswith('.dll') else name)
            if name.endswith('.dll') and binary is not None: source=binary
            archive.write(source,root+name)
        for name in sorted(set(bundle['files'])|{'bundle.json'}):archive.write(runtime/name,root+'runtime/'+name)
        archive.write(P/'ports/windows-client/README.md',root+'README.md')
        archive.write(P/'THIRD_PARTY_NOTICES.md',root+'THIRD_PARTY_NOTICES.md')
        archive.write(P/'LICENSE',root+'LICENSE')
    return dict(package=str(output),sha256=hashlib.sha256(output.read_bytes()).hexdigest(),runtimeIdentity=bundle['identity'],stable=False)

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--binary',type=Path);a=p.parse_args()
    print(json.dumps(package(a.runtime,a.output,a.binary),indent=2))
