"""Run the bundled worker without system site packages, using a verified cached seed."""
import argparse,json,subprocess,tempfile,hashlib
from pathlib import Path
from check_world import check

def run(mod):
    mod=mod.resolve();runtime=mod/'runtime'
    game_line=json.loads((runtime/'game-target.json').read_bytes())['gameLine']
    with tempfile.TemporaryDirectory(prefix='worker-test-',dir=mod) as tmp:
        root=Path(tmp)
        def invoke(name,seed,title,line=game_line):
            job=root/name;job.mkdir(exist_ok=True)
            (job/'request.json').write_text(json.dumps(dict(seed=seed,name=title,gameLine=line),ensure_ascii=False),'utf-8')
            result=subprocess.run([str(runtime/'python/python.exe'),'-I','-B','-X','utf8',
                                   str(runtime/'ports/windows-client/seed_service.py'),str(job/'request.json')],
                                  cwd=runtime,capture_output=True,text=True,encoding='utf-8',timeout=600)
            return job,result,json.loads((job/'status.json').read_bytes())
        a,r,s=invoke('a','288493980','中文世界 A')
        assert r.returncode==0,(r.stdout,r.stderr)
        assert s['state']=='ready' and check(a/'prepared')['seed']=='288493980'
        b,r,s=invoke('b','288493980','中文世界 B')
        assert r.returncode==0 and check(b/'prepared')['name']=='中文世界 B',r.stderr
        digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
        assert digest(a/'prepared/karma/native/native-input.json')==digest(b/'prepared/karma/native/native-input.json')
        marker=digest(a/'prepared/karma-worldgen.json')
        _,r,s=invoke('a','288493980','不可覆盖')
        assert r.returncode!=0 and s['state']=='error'
        assert marker==digest(a/'prepared/karma-worldgen.json')
        for seed in ['9223372036854775808','1;exit','001']:
            job,r,s=invoke('bad-'+str(len(seed)),seed,'非法种子')
            assert r.returncode!=0 and s['state']=='error' and not (job/'prepared').exists()
        job,r,s=invoke('wrong-target','288493980','不兼容目标','26.20' if game_line=='26.51' else '26.51')
        assert r.returncode!=0 and s['state']=='error' and not (job/'prepared').exists()
    return dict(cachedWorlds=2,identicalTerrain=True,overwriteRejected=True,invalidSeedsRejected=3,
                isolatedBundledPython=True,targetMismatchRejected=True,gameTested=False)

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('mod',type=Path);a=p.parse_args();print(json.dumps(run(a.mod)))
