"""Compare cold exploration, calculator queues, output hashes and completion gaps."""
import argparse
import json
import shutil
import statistics
import subprocess
from pathlib import Path


def run(executable, recipe, route, destination, workers):
    destination.mkdir()
    shutil.copy2(recipe, destination / 'recipe.json')
    shutil.copy2(route, destination / 'route.json')
    reports, hashes = {}, {}
    for state in ('cold', 'warm'):
        report_path = destination / (state + '.json')
        process = subprocess.run(
            [str(executable), str(destination / 'recipe.json'), str(destination / 'route.json'),
             str(report_path), str(destination / 'regional-cache'), str(workers)],
            capture_output=True, text=True, check=True, timeout=600)
        report = json.loads(report_path.read_bytes())
        rows = report.pop('rows')
        hashes[state] = [(row['chunk'], row['hash']) for row in rows]
        if report['chunks'] != len(rows):
            raise RuntimeError('Missing exploration results')
        reports[state] = report
        (destination / (state + '.log')).write_text(process.stdout + process.stderr, 'utf-8')
    if hashes['cold'] != hashes['warm']:
        raise RuntimeError('Reopened regional plans changed chunks')
    return reports, hashes['cold']


def compare(args):
    args.output.mkdir(parents=True, exist_ok=False)
    report = dict(scope='offline terrain kernel and calculator pool; not in-game acceptance',
                  workers=args.workers, runs=[], byteIdentical=False, passed=False)
    baseline_hashes = None
    for repeat in range(args.runs):
        rows = {}
        names = ['baseline', 'candidate'] if repeat % 2 == 0 else ['candidate', 'baseline']
        for name in names:
            rows[name], hashes = run(getattr(args, name), args.recipe, args.route,
                                     args.output / f'{repeat}-{name}', args.workers)
            if baseline_hashes is None:
                baseline_hashes = hashes
            elif hashes != baseline_hashes:
                raise RuntimeError('Version, cache state or execution order changed generated chunks')
            print(json.dumps(dict(repeat=repeat, version=name,
                                  coldMs=rows[name]['cold']['wallMs'], warmMs=rows[name]['warm']['wallMs'],
                                  coldGapMs=rows[name]['cold']['maxNoCompletionAfterFirstMs'])), flush=True)
        report['runs'].append(rows)
        (args.output / 'report.json').write_text(json.dumps(report, indent=2), 'utf-8')
    checks = []
    for state in ('cold', 'warm'):
        for field in ('wallMs', 'firstCompletedMs', 'maxNoCompletionMs', 'maxNoCompletionAfterFirstMs'):
            # Throughput uses a median. Stalls use the worst observed run, so
            # several fast runs cannot hide a single long no-completion gap.
            aggregate = statistics.median if field == 'wallMs' else max
            values = {name: aggregate(row[name][state][field] for row in report['runs'])
                      for name in ('baseline', 'candidate')}
            tolerance = 0 if field == 'wallMs' else args.gap_tolerance_ms
            limit = values['baseline'] * args.max_ratio + tolerance
            checks.append(dict(cache=state, metric=field, aggregate='median' if field == 'wallMs' else 'max',
                               **values, limit=limit, passed=values['candidate'] <= limit))
    report.update(byteIdentical=True, checks=checks, passed=all(row['passed'] for row in checks))
    (args.output / 'report.json').write_text(json.dumps(report, indent=2), 'utf-8')
    print(json.dumps(dict(byteIdentical=True, passed=report['passed'], checks=checks)), flush=True)
    if not report['passed']:
        raise RuntimeError('Exploration throughput or longest completion gap regressed')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('baseline', 'candidate', 'recipe', 'route', 'output'):
        parser.add_argument('--' + name, required=True, type=lambda p: Path(p).resolve())
    parser.add_argument('--runs', type=int, default=3)
    parser.add_argument('--workers', type=int, choices=range(1, 5), default=4)
    parser.add_argument('--max-ratio', type=float, default=1.05)
    parser.add_argument('--gap-tolerance-ms', type=float, default=100)
    args = parser.parse_args()
    if args.runs < 1 or args.max_ratio <= 0 or args.gap_tolerance_ms < 0:
        parser.error('invalid run count or regression limits')
    compare(args)
