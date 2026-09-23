"""Purpose: run the discovered C executable roster and retain every verdict.
Owns resources: joins all child processes; logs and temporary files stay in this tree.
Guarantees: no failed or unchecked process counts as success [tested: make check; commit=6022c3f48b6dc64752c6e49cfe9d985c7ac7a4e9].
Open Obligations: None.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def execute(name):
    """Run without a deadline and keep the complete output for diagnosis."""
    log = ROOT / "build/logs" / (name.replace("/", "_") + ".log")
    env = dict(os.environ, TMPDIR=str(ROOT / "ai-tmp"))
    with log.open("w") as output:
        result = subprocess.run([str(ROOT / name)], cwd=ROOT, env=env,
                                stdout=output, stderr=subprocess.STDOUT)
    checked = any(line.startswith("OK ") for line in log.read_text(errors="replace").splitlines())
    return {"program": name, "exit": result.returncode, "checked": checked,
            "log": str(log.relative_to(ROOT))}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--jobs", type=int, default=1)
    parser.add_argument("programs", nargs="+")
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    (ROOT / "ai-tmp").mkdir(exist_ok=True)
    (ROOT / "build/logs").mkdir(parents=True, exist_ok=True)
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        results = list(pool.map(execute, args.programs))
    (ROOT / "build/results.json").write_text(json.dumps(results, indent=2) + "\n")
    failed = [r for r in results if r["exit"] or not r["checked"]]
    for result in failed:
        print(f'FAIL {result["program"]}: exit {result["exit"]}; {result["log"]}')
    print(f"{len(results) - len(failed)}/{len(results)} examples passed")
    raise SystemExit(bool(failed))


if __name__ == "__main__":
    main()
