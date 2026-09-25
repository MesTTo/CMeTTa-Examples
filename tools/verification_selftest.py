"""Purpose: prove tools/verification.py can come back red. Each case plants a
record or a gate output in a scratch repository and asks the writer or the
checker for its verdict, so a writer that stopped rewriting a root, or let a
machine path through, or a checker that stopped seeing one, fails here
instead of passing every record.

Assumes: git and `swipl` on PATH, as `make check` has them.
Guarantees: exits nonzero unless every planted case is judged as expected
[tested 2026-09-25T22:53:52+10:00: with verification.relative and HOME_PATH
disabled it read 4/11 and exited 1; intact it reads 11/11].
Owns resources: a scratch repository under ai-tmp/, removed on success and
kept on failure for inspection.
"""

from __future__ import annotations

import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import verification as record  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
SCRATCH = ROOT / "ai-tmp" / "verification-selftest"
# Built from parts, so this file names no home-directory path itself and the
# checker it tests holds it to the rule like any other tracked file.
HOMES = Path("/home"), Path("/Users")
ENGINE = HOMES[0] / "verifier" / "MeTTa"


def refused(action) -> bool:
    try:
        action()
    except SystemExit as stop:
        return stop.code not in (None, 0)
    return False


def planted(files: dict[str, str]) -> Path:
    """A scratch repository tracking exactly these files."""
    repo = SCRATCH / str(len(list(SCRATCH.glob("*"))) if SCRATCH.exists() else 0)
    repo.mkdir(parents=True)
    for name, text in files.items():
        (repo / name).parent.mkdir(parents=True, exist_ok=True)
        (repo / name).write_text(text, encoding="utf-8")
    subprocess.run(["git", "init", "-q", str(repo)], check=True)
    subprocess.run(["git", "-C", str(repo), "add", "-A"], check=True)
    return repo


def gate(lines: list[str]) -> Path:
    path = planted({"gate.log": "\n".join(lines) + "\n"}) / "gate.log"
    return path


BLOCK = f"{record.BEGIN}\n{record.END}\n"
CASES = {
    "a path under the engine tree is written relative to it":
        lambda: record.relative(f"installed cmetta 1.0.0 under {ENGINE}/extensions/cmetta/build/install-check",
                                [ENGINE, ROOT])
        == "installed cmetta 1.0.0 under extensions/cmetta/build/install-check",
    "the engine root itself is written as .":
        lambda: record.relative(f"engine {ENGINE}, then more", [ENGINE, ROOT]) == "engine ., then more",
    "a sibling sharing the root's prefix keeps its name":
        lambda: record.relative(f"{ENGINE}2/lib", [ENGINE, ROOT]) == f"{ENGINE}2/lib",
    "a path under this corpus is written relative to it":
        lambda: record.relative(f"OK {ROOT}/integration/installed_consumer.c", [ENGINE, ROOT])
        == "OK integration/installed_consumer.c",
    "a gate line naming another home-directory path is refused":
        lambda: refused(lambda: record.component(gate(["ok", f"under {HOMES[0] / 'someone' / 'prefix' / 'lib'}"]),
                                                 [ENGINE, ROOT])),
    "a macOS home path is refused too":
        lambda: refused(lambda: record.component(gate([f"under {HOMES[1] / 'someone' / 'prefix' / 'lib'}"]),
                                                 [ENGINE, ROOT])),
    "a gate output inside the roots is kept":
        lambda: record.component(gate([f"under {ENGINE}/extensions/cmetta/build", "GATE c-binding ok"]),
                                 [ENGINE, ROOT]) == "under extensions/cmetta/build\nGATE c-binding ok\n",
    "the checker names a tracked line with a home-directory path":
        lambda: record.check(planted({"VERIFICATION.md": BLOCK,
                                      "notes.txt": f"the host at {HOMES[0] / 'me' / 'swipl' / 'bin'}\n"})) != [],
    "the checker passes a record with none":
        lambda: record.check(planted({"VERIFICATION.md": BLOCK, "notes.txt": "the host, SWI-Prolog 10.1.14\n"})) == [],
    "the checker names a record without its host block":
        lambda: record.check(planted({"VERIFICATION.md": "no block here\n"})) != [],
    "the host block names the host by identity and no path":
        lambda: (lambda block: block.startswith("- SWI-Prolog ") and not record.HOME_PATH.search(block))(
            record.host_block("swipl", "cc")),
}


def main() -> int:
    shutil.rmtree(SCRATCH, ignore_errors=True)
    wrong = []
    for name, judged in CASES.items():
        try:
            if not judged():
                wrong.append(name)
        except Exception as error:  # a case that raises was not judged as expected
            wrong.append(f"{name}: {type(error).__name__}: {error}")
    for case in wrong:
        print(f"SELFTEST {case}")
    print(f"{len(CASES) - len(wrong)}/{len(CASES)} planted cases judged as expected")
    if wrong:
        return 1
    shutil.rmtree(SCRATCH, ignore_errors=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
