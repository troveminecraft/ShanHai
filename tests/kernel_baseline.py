"""Record/compare exact output and measured CLI latency; never a gameplay test.

Use the same recipe, executable build settings and coordinates for comparisons.
Each invocation gets its own regional-cache directory. 'warm' means repeating
requests in the same process, not the BDS disk/memory chunk cache or network.
"""
import argparse
import hashlib
import json
import math
import platform
import shutil
import subprocess
import time
from pathlib import Path


def sha(data):
    return hashlib.sha256(data).hexdigest()


def summary(values):
    ordered = sorted(values)
    return {"count": len(values), "sum": sum(values), **{
        f"p{p}": ordered[max(0, math.ceil(len(ordered) * p / 100) - 1)]
        for p in (50, 95, 99)}}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, required=True)
    parser.add_argument("--recipe", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--compare", type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    recipe = args.output.resolve() / "native-input.json"
    shutil.copyfile(args.recipe, recipe)
    # Fixed ordinary, adjacent, reported problem and far regional coordinates.
    coords = [(0, 0), (1, 0), (-1, -1), (103, -142), (102, -142),
              (175, 539), (175, 540), (-58, 25), (-94, -374),
              (68, 50), (-128, 127), (256, -257)]
    queries = [{"spawnProbe": [x + 32768, z + 32768]}
               for z in range(-4096, 4097, 512) for x in range(-4096, 4097, 512)]
    queries += [{"cx": x, "cz": z, "output": str(args.output.resolve() / f"{phase}-{x}-{z}.bin")}
                for phase in ("cold", "warm") for x, z in coords]
    start = time.perf_counter()
    proc = subprocess.run([str(args.exe.resolve()), str(recipe)],
                          input="".join(json.dumps(q) + "\n" for q in queries),
                          capture_output=True, text=True, encoding="utf-8", timeout=900)
    elapsed = (time.perf_counter() - start) * 1000
    (args.output / "stderr.txt").write_text(proc.stderr, encoding="utf-8")
    proc.check_returncode()
    rows = [json.loads(row) for row in proc.stdout.splitlines()]
    if len(rows) != len(queries):
        raise RuntimeError("Missing responses")
    probes, chunks = rows[:289], rows[289:]
    hashes = []
    for x, z in coords:
        cold = (args.output / f"cold-{x}-{z}.bin").read_bytes()
        warm = (args.output / f"warm-{x}-{z}.bin").read_bytes()
        if cold != warm or cold[:4] != b"KW2\0":
            raise RuntimeError(f"Warm output differs or invalid header at {x},{z}")
        hashes.append({"cx": x, "cz": z, "bytes": len(cold), "sha256": sha(cold)})
    n = len(coords)
    report = {"schema": 1, "recipeSha256": sha(recipe.read_bytes()),
              "executableSha256": sha(args.exe.read_bytes()),
              "machine": platform.machine(), "os": platform.platform(),
              "probes": probes, "chunks": hashes,
              "timingsMs": {"processWall": elapsed,
                            "cold": summary([row["ms"] for row in chunks[:n]]),
                            "warm": summary([row["ms"] for row in chunks[n:]])},
              "chunkTimings": chunks,
              "measurementScope": "single CLI process; cold includes warmed surface-probe caches; warm repeats chunks; no BDS/cache/queue/network/gameplay measurement",
              "gameplayTestPerformed": False}
    if args.compare:
        before = json.loads(args.compare.read_text(encoding="utf-8"))
        for key in ("schema", "recipeSha256", "probes", "chunks"):
            if report[key] != before[key]:
                raise RuntimeError(f"Baseline mismatch: {key}")
        report["baselineEqual"] = True
        report["baselineReportSha256"] = sha(args.compare.read_bytes())
    (args.output / "result.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(json.dumps({"probes": len(probes), "chunks": n, "timingsMs": report["timingsMs"],
                      "baselineEqual": report.get("baselineEqual")}), flush=True)


if __name__ == "__main__":
    main()
