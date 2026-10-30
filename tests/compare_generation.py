"""Compare cold/warm generation cost and exact chunks between two native CLIs."""
import argparse
import hashlib
import json
import shutil
import statistics
import subprocess
import time
from pathlib import Path


def run(executable, recipe, chunks, destination):
    destination.mkdir()
    local_recipe = destination / 'native-input.json'
    shutil.copy2(recipe, local_recipe)
    requests = [dict(cx=x, cz=z, output=str(destination / f'{x}_{z}.kw2')) for x, z in chunks]
    payload = ''.join(json.dumps(q) + '\n' for q in requests)
    results = []
    for state in ('cold', 'warm'):
        started = time.perf_counter()
        process = subprocess.run([str(executable), str(local_recipe)], input=payload,
                                 capture_output=True, text=True, check=True, timeout=600)
        elapsed = time.perf_counter() - started
        rows = [json.loads(line) for line in process.stdout.splitlines()]
        if len(rows) != len(chunks):
            raise RuntimeError('Missing chunk results')
        (destination / f'{state}.jsonl').write_text(process.stdout, 'utf-8')
        hashes = [hashlib.sha256(Path(q['output']).read_bytes()).hexdigest() for q in requests]
        results.append(dict(cache=state, wallSeconds=elapsed,
                            generationMs=sum(row['ms'] for row in rows), hashes=hashes))
    if results[0]['hashes'] != results[1]['hashes']:
        raise RuntimeError('Warm regional plans changed generated chunks')
    return results


def compare(baseline, candidate, recipe, chunks, output, runs, max_ratio):
    output.mkdir(parents=True, exist_ok=False)
    report = dict(runs=[], chunks=chunks, maxColdRatio=max_ratio)
    for repeat in range(runs):
        result = {}
        # Alternate execution order to reduce systematic warm-machine bias.
        order = [('baseline', baseline), ('candidate', candidate)]
        if repeat % 2:
            order.reverse()
        for label, executable in order:
            result[label] = run(executable, recipe, chunks, output / f'{repeat}-{label}')
        if result['baseline'][0]['hashes'] != result['candidate'][0]['hashes']:
            raise RuntimeError('Candidate changed generated chunks')
        report['runs'].append(result)
        (output / 'report.json').write_text(json.dumps(report, indent=2), 'utf-8')
    cold = {name: statistics.median(row[name][0]['generationMs'] for row in report['runs'])
            for name in ('baseline', 'candidate')}
    report.update(byteIdentical=True, medianColdMs=cold,
                  coldRatio=cold['candidate'] / cold['baseline'])
    (output / 'report.json').write_text(json.dumps(report, indent=2), 'utf-8')
    print(json.dumps({k: v for k, v in report.items() if k not in ('runs', 'chunks')}), flush=True)
    if report['coldRatio'] > max_ratio:
        raise RuntimeError('Cold generation exceeded the allowed performance ratio')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('baseline', 'candidate', 'recipe', 'chunks', 'output'):
        parser.add_argument('--' + name, required=True, type=Path)
    parser.add_argument('--runs', type=int, default=3)
    parser.add_argument('--max-ratio', type=float, default=1.05)
    args = parser.parse_args()
    if args.runs < 1 or args.max_ratio <= 0:
        parser.error('runs and max-ratio must be positive')
    coords = json.loads(args.chunks.read_bytes())
    if not coords or any(len(p) != 2 or any(type(v) is not int for v in p) for p in coords):
        parser.error('chunks must contain a non-empty array of integer [cx, cz] pairs')
    compare(args.baseline.resolve(), args.candidate.resolve(), args.recipe.resolve(),
            coords, args.output.resolve(), args.runs, args.max_ratio)
