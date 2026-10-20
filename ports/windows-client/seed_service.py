"""Hidden per-request worker. Does not create, open, or modify a player's world.

Only a verified staging package is returned to the UI-thread creation adapter.
The owning Windows Job Object terminates this worker and children on cancellation.
"""
import argparse, hashlib, json, os, subprocess, sys, time
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from prepare_world import prepare, sha, P

def atomic_json(path, value):
    temporary = path.with_suffix('.tmp')
    temporary.write_text(json.dumps(value, ensure_ascii=False), 'utf-8')
    os.replace(temporary, path)

def run(request_path):
    request_path = request_path.resolve()
    request = json.loads(request_path.read_bytes())
    job = request_path.parent
    status = job/'status.json'
    seed = request['seed']
    if not isinstance(seed, str) or str(int(seed)) != seed or not -(1<<63) <= int(seed) < (1<<63):
        raise ValueError('Invalid signed 64-bit seed')
    bundle = json.loads((P/'bundle.json').read_bytes())
    # All destinations are derived from the installed helper, never supplied by UI.
    mod = P.parent
    cache = mod/'seed-cache'/bundle['identity']/seed
    cache.mkdir(parents=True, exist_ok=True)
    source = cache/'native-input.json'
    receipt = cache/'receipt.json'
    usable = False
    if source.is_file() and receipt.is_file():
        r = json.loads(receipt.read_bytes())
        usable = r.get('identity') == bundle['identity'] and r.get('sha256') == sha(source) and r.get('generationMode')=='regional-on-demand'
    env = os.environ.copy()
    env.pop('PYTHONHOME', None)
    env.pop('PYTHONPATH', None)
    env['PATH'] = str(P/'node') + os.pathsep + env.get('PATH', '')
    env['PYTHONUTF8'] = '1'
    env['PYTHONNOUSERSITE'] = '1'
    env['OPENBLAS_NUM_THREADS'] = '1'
    env['OMP_NUM_THREADS'] = '1'
    if not usable:
        atomic_json(status, dict(state='running', phase='正在准备新种子的地形数据',progress=0.,progressMode='stage-weighted'))
        output = job/'native-input.json'
        subprocess.run([sys.executable, '-X', 'utf8', str(P/'tools/build_streaming_seed.py'),
                        '--seed', seed, '--output', str(output),
                        '--status', str(status)], cwd=P, env=env, check=True)
        # Publication is last; a cancelled worker never leaves a trusted partial cache.
        os.replace(output, source)
        atomic_json(receipt, dict(identity=bundle['identity'], sha256=sha(source),generationMode='regional-on-demand'))
    atomic_json(status, dict(state='running', phase='正在准备出生点周围区域',progress=.65,progressMode='stage-weighted'))
    spawn = None
    spawn_path = cache/'spawn-check.json'
    if spawn_path.is_file():
        candidate = json.loads(spawn_path.read_bytes())
        generator = P/'build/windows/x64/release/native-worldgen-check.exe'
        if candidate.get('inputSha256') == sha(source) and candidate.get('generatorSha256') == sha(generator):
            spawn = candidate
    result = prepare(source, P/'void-level.dat', job/'prepared', request['name'],
                     mod/'karma-worldgen-client.dll', spawn_report=spawn)
    atomic_json(spawn_path, json.loads((job/'prepared/karma/spawn-check.json').read_bytes()))
    if result['seed'] != seed:
        raise ValueError('Prepared seed differs from requested seed')
    atomic_json(status, dict(state='ready', phase='准备完成', progress=1.,seed=seed, spawn=result['spawn']))

if __name__ == '__main__':
    p=argparse.ArgumentParser(); p.add_argument('request', type=Path); a=p.parse_args()
    try:
        run(a.request)
    except Exception as e:
        atomic_json(a.request.parent/'status.json', dict(state='error', phase='准备失败，请返回重试', detail=str(e)))
        raise
