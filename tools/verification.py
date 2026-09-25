"""Purpose: write the parts of the verification record a run determines, so none
is copied by hand and no line names a path only the verifying machine has:
VERIFICATION.md's host block, read from the SWI-Prolog and toolchain the run
used; verification/component.txt, the C seat gate's output with the engine's
and this corpus's roots written repository-relative; and, after `make check`,
the lane's receipts copied from build/. --check refuses a tracked line naming a
path in a home directory, and a VERIFICATION.md without its host block.

Assumes: --engine, given with --gate, names the engine tree the run used,
  whose absolute path is the prefix its gate output wrote; `swipl` on PATH is
  the host the run used, which names its declared patches in
  <home>/metta-host.pl [source 2026-09-25T22:48:46+10:00:
  swipl-patched.6/lib/swipl/metta-host.pl, host_build/1 and host_patch/2,
  written by the engine's tools/pymetta-host/declare-host.sh]; `cc` is the
  compiler the Makefile's CC defaults to.
Guarantees:
  - the host is named by its identity, SWI-Prolog's version flag, the build
    its declaration names and how many patches it declares, never by the
    directory it is installed in
  - a gate line naming a home-directory path outside the engine tree and this
    corpus is refused with the line, so the writer never ships one silently
  - --check exits nonzero naming each tracked line that names a path in a home
    directory, which is what the release refuses to publish
  [tested 2026-09-25T23:00:22+10:00: tools/verification_selftest.py, 13 planted
  cases]
Fails when: the gate output names a machine path outside the engine tree and
  this corpus, which it refuses until the lane that printed it is fixed.
"""

from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BEGIN, END = "<!-- host:begin -->", "<!-- host:end -->"
# The part of a line only the verifying machine has: a path into a home
# directory, on Linux or macOS, or a Windows checkout's root. It is the
# release's own rule, which refuses any tracked line holding the Linux form,
# widened as the engine's own tracked-path test widens it: a Windows root is
# one drive letter, which the lookbehind requires, so a URI scheme's tail is
# not read as a drive [source 2026-09-25T22:59:40+10:00: the MeTTa checkout's
# extensions/python/tests/repository/test_workspace_paths.py, _WINDOWS_ROOT].
HOME_PATH = re.compile(r"/(home|Users)/|(?<![A-Za-z])[A-Za-z]:[\\/](Users|home|a)[\\/]")
IDENTITY_GOAL = (
    "current_prolog_flag(version, V), current_prolog_flag(compiled_at, C), "
    "current_prolog_flag(home, H), atom_concat(H, '/metta-host.pl', F), "
    "( exists_file(F) -> load_files(F, [silent(true)]), host_build(B), "
    "aggregate_all(count, host_patch(_, _), N) ; B = none, N = none ), "
    "format('~w~n~w~n~w~n~w~n', [V, C, B, N])"
)


def host_block(swipl: str, cc: str) -> str:
    """The host and toolchain the run used, one list line each."""
    answered = subprocess.run(
        [swipl, "-q", "-g", IDENTITY_GOAL, "-t", "halt"],
        stdin=subprocess.DEVNULL, capture_output=True, text=True, check=True,
    ).stdout.splitlines()
    version, compiled, build, patches = answered
    number = int(version)
    release = f"{number // 10000}.{number // 100 % 100}.{number % 100}"
    if build == "none":
        swi = f"- SWI-Prolog {release}, compiled {compiled}, with no host declaration"
    elif build != compiled:
        sys.exit(f"verification: {swipl} was compiled {compiled} but declares the build {build}")
    else:
        swi = f"- SWI-Prolog {release}, the build of {compiled}, declaring {patches} host patches"
    versions = [
        subprocess.run(command, capture_output=True, text=True, check=True).stdout.splitlines()[0].strip()
        for command in ([cc, "--version"], ["cmake", "--version"], [sys.executable, "--version"])
    ]
    return "\n".join([swi] + [f"- {line}" for line in versions])


def write_host(record: Path, block: str) -> None:
    text = record.read_text(encoding="utf-8")
    start, end = text.find(BEGIN), text.find(END)
    if not 0 <= start < end:
        sys.exit(f"verification: {record.name} has no {BEGIN} ... {END} block")
    record.write_text(text[:start + len(BEGIN)] + "\n" + block + "\n" + text[end:], encoding="utf-8")


def relative(line: str, roots: list[Path]) -> str:
    """The line with each root written repository-relative: a path under a
    root loses the root, and the root itself becomes `.`. A root is replaced
    only where the path ends with it, so a sibling that shares its prefix,
    `/a/b` beside `/a/bc`, keeps its own name."""
    for root in sorted(roots, key=lambda path: len(str(path)), reverse=True):
        line = line.replace(f"{root}/", "")
        line = re.sub(re.escape(str(root)) + r"(?![\w.-])", ".", line)
    return line


def component(gate: Path, roots: list[Path]) -> str:
    """The gate's output as the record keeps it, or a refusal naming the lines
    that would still name a machine path."""
    lines = [relative(line, roots) for line in gate.read_text(encoding="utf-8").splitlines()]
    named = [f"{number}: {line}" for number, line in enumerate(lines, 1) if HOME_PATH.search(line)]
    if named:
        sys.exit("verification: the gate output names a machine path outside the engine and "
                 "the corpus:\n" + "\n".join(named))
    return "\n".join(lines) + "\n"


def copy_receipts(root: Path) -> None:
    for name in ("results.json", "twins.json"):
        receipt = root / "build" / name
        if not receipt.is_file():
            sys.exit(f"verification: build/{name} is missing; run make check first")
        shutil.copy(receipt, root / "verification" / name)


def check(root: Path) -> list[str]:
    """What keeps the record from shipping: every tracked line naming a
    home-directory path, and a VERIFICATION.md without its host block."""
    # The tracked files as the working tree holds them, read by the pattern
    # the writer refuses with; a file that is not text names no path.
    tracked = subprocess.run(["git", "-C", str(root), "ls-files", "-z"],
                             capture_output=True, text=True, check=True).stdout.split("\0")
    wrong = []
    for name in filter(None, tracked):
        try:
            text = (root / name).read_text(encoding="utf-8")
        except (UnicodeDecodeError, OSError):
            continue
        wrong += [f"a tracked line names a path in a home directory: {name}:{number}: {line.strip()[:120]}"
                  for number, line in enumerate(text.splitlines(), 1) if HOME_PATH.search(line)]
    text = (root / "VERIFICATION.md").read_text(encoding="utf-8")
    if not 0 <= text.find(BEGIN) < text.find(END):
        wrong.append(f"VERIFICATION.md has no {BEGIN} ... {END} block")
    return wrong


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--engine", type=Path, help="the engine tree the run used, with --gate")
    parser.add_argument("--gate", type=Path, help="the C seat gate's output from the run")
    parser.add_argument("--receipts", action="store_true", help="copy build/'s receipts")
    parser.add_argument("--swipl", default="swipl")
    parser.add_argument("--cc", default="cc")
    parser.add_argument("--check", action="store_true")
    arguments = parser.parse_args()
    if not arguments.check:
        (ROOT / "verification").mkdir(exist_ok=True)
        if arguments.gate is not None:
            if arguments.engine is None:
                parser.error("--gate needs --engine, the tree whose paths it wrote")
            kept = component(arguments.gate, [arguments.engine.resolve(), ROOT])
            (ROOT / "verification" / "component.txt").write_text(kept, encoding="utf-8")
        if arguments.receipts:
            copy_receipts(ROOT)
        write_host(ROOT / "VERIFICATION.md", host_block(arguments.swipl, arguments.cc))
    wrong = check(ROOT)
    for line in wrong:
        print(line, file=sys.stderr)
    return 1 if wrong else 0


if __name__ == "__main__":
    raise SystemExit(main())
