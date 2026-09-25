"""Purpose: write the parts of the verification record a run determines, so none
is copied by hand, none describes a different run from the rest, and no line
names a path only the verifying machine has. From one run's three sources, the
C seat gate's output, the make output of the corpus steps the Commands section
lists, and the receipts `make check` leaves in build/, it writes VERIFICATION.md's
identity paragraph (the corpus and engine commits, read from git, and the
cmetta version the gate printed), its host block (the SWI-Prolog and toolchain
the run used) and its Results table (each cell the verdict line the step's own
tool printed); verification/component.txt, the gate's output with the engine's
and this corpus's roots written repository-relative; and the receipts. --check
refuses a tracked line naming a path in a home directory, and a VERIFICATION.md
without its three blocks.

Assumes: --engine names the engine tree the run used, a git checkout of its
  own with its components checked out, whose absolute path is the prefix its
  gate output and the make output wrote; `swipl` on PATH is
  the host the run used, which names its declared patches in
  <home>/metta-host.pl [source 2026-09-25T22:48:46+10:00:
  swipl-patched.6/lib/swipl/metta-host.pl, host_build/1 and host_patch/2,
  written by the engine's tools/pymetta-host/declare-host.sh]; `cc` is the
  compiler the Makefile's CC defaults to; the make output holds each recipe
  line as make echoed it, which it does for every line not marked `@`
  [source: GNU make manual, 5.2 Recipe Echoing].
Guarantees:
  - nothing is written unless every part can be: a step whose verdict line
    the make output lacks, a receipt whose count differs from the line its
    tool printed, a program or translation unit the make output never built,
    a corpus checkout holding anything but its commit's files, an engine
    checkout with a tracked change, a tree that is not a git checkout of its
    own, or two cmetta versions in the gate output stops the writer naming
    it, so the record never mixes two runs or names another repository's
    commit
  - every quoted line has the engine's and this corpus's roots written
    repository-relative, and a line naming another home-directory path is
    refused with the line, so the writer never ships one silently
  - the host is named by its identity, SWI-Prolog's version flag, the build
    its declaration names and how many patches it declares, never by the
    directory it is installed in
  - --check exits nonzero naming each tracked line that names a path in a home
    directory, which is what the release refuses to publish
  [tested 2026-09-26T03:22:18+10:00: tools/verification_selftest.py, 38 planted
  cases]
Fails when: the gate output names a machine path outside the engine tree and
  this corpus, which it refuses until the lane that printed it is fixed; or a
  tool's verdict line changes shape, which it refuses until STEPS or
  GATE_DETAIL follows it.
Decides: STEPS and GATE_DETAIL, which line of each tool's output the record
  quotes.
"""

from __future__ import annotations

import argparse
import json
import re
import shutil
import subprocess
import sys
import textwrap
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BLOCKS = ("identity", "host", "results")
# The files this writer writes, which a checkout may hold changed from an
# earlier run of it and still be at its commit for everything the run built.
WRITTEN = ("VERIFICATION.md", "verification/component.txt",
           "verification/results.json", "verification/twins.json")
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
# The Results rows a tool's own verdict line answers: the row, the recipe line
# whose echo opens the tool's output, and the shape of the line it prints for
# its verdict. The two self-tests print the same shape, so the echo that
# precedes a line, and not the line, says which tool printed it.
STEPS = (
    ("embedding programs", r"python3 tools/run\.py ", r"\d+/(\d+) examples passed"),
    ("twin lane", r"python3 tools/twin_lane\.py ", r"\d+/(\d+) twins agree with their originals; .*"),
    ("lane self-test", r"python3 tools/twin_lane_selftest\.py ", r"\d+/\d+ planted cases judged as expected"),
    ("index", r"python3 tools/index\.py .*--check", r"index, coverage and README verified"),
    ("record self-test", r"python3 tools/verification_selftest\.py", r"\d+/\d+ planted cases judged as expected"),
)
# The receipt each count-bearing verdict line describes: the line's total must
# be the receipt's length, or the two came from different runs.
RECEIPTS = {"embedding programs": "results.json", "twin lane": "twins.json"}
# Each gate lane whose output holds a line worth quoting beside its verdict,
# and the shapes of those lines, matched from the line's start; a shape with a
# group quotes the group. A lane not named here is quoted by its verdict.
GATE_DETAIL = {
    "c-binding": (r"surface: .*", r"\d+ checks, \d+ failures"),
    "c-bench": (r"\S+: (\S+ instruction regression: .*)", r"(\d+ comparison\(s\) declined)",
                (r"NOT MEASURED IN THIS CONFIGURATION \S+ [\w ]+ not compared; "
                 r"(checkout length \d+, depth \d+; canonical length \d+, depth \d+)")),
    "llms": (r"llms: (.*)",),
    "evidence": (r"\d+ unbacked evidence tag\(s\) in \d+ claims",),
}
VERSION = re.compile(r"version: (\S+), and mt_version\(\) agrees")


def markers(name: str) -> tuple[str, str]:
    return f"<!-- {name}:begin -->", f"<!-- {name}:end -->"


def refuse(why: str) -> None:
    sys.exit(f"verification: {why}")


def git(repo: Path, *args: str) -> str:
    return subprocess.run(["git", "-C", str(repo), *args], capture_output=True, text=True,
                          check=True).stdout.rstrip("\n")


def commit_of(tree: Path) -> str:
    """The short commit a checkout holds. A directory that is not a checkout
    of its own is refused: git run inside it reads the nearest enclosing
    repository, whose commit names something else, as an archived tree inside
    another checkout would."""
    # A directory in no repository at all answers nonzero and prints nothing.
    top = subprocess.run(["git", "-C", str(tree), "rev-parse", "--show-toplevel"],
                         capture_output=True, text=True, check=False).stdout.strip()
    if not top or Path(top).resolve() != tree.resolve():
        refuse(f"{tree} is not a git checkout of its own{f' (git reads {top} there)' if top else ''}; "
               "verify against a checkout, whose commits the record can name")
    return git(tree, "rev-parse", "--short", "HEAD")


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
        refuse(f"{swipl} was compiled {compiled} but declares the build {build}")
    else:
        swi = f"- SWI-Prolog {release}, the build of {compiled}, declaring {patches} host patches"
    versions = [
        subprocess.run(command, capture_output=True, text=True, check=True).stdout.splitlines()[0].strip()
        for command in ([cc, "--version"], ["cmake", "--version"], [sys.executable, "--version"])
    ]
    return "\n".join([swi] + [f"- {line}" for line in versions])


def write_block(text: str, name: str, block: str) -> str:
    """The record with the text between NAME's markers replaced by BLOCK."""
    begin, end = markers(name)
    start, stop = text.find(begin), text.find(end)
    if not 0 <= start < stop:
        refuse(f"VERIFICATION.md has no {begin} ... {end} block")
    return text[:start + len(begin)] + "\n" + block + "\n" + text[stop:]


def relative(line: str, roots: list[Path]) -> str:
    """The line with each root written repository-relative: a path under a
    root loses the root, and the root itself becomes `.`. A root is replaced
    only where the path ends with it, so a sibling that shares its prefix,
    `/a/b` beside `/a/bc`, keeps its own name."""
    for root in sorted(roots, key=lambda path: len(str(path)), reverse=True):
        line = line.replace(f"{root}/", "")
        line = re.sub(re.escape(str(root)) + r"(?![\w.-])", ".", line)
    return line


def kept(lines: list[str], source: str) -> list[str]:
    """The lines, already relative, as the record keeps them, or a refusal
    naming the ones that would still name a machine path."""
    named = [f"{number}: {line}" for number, line in enumerate(lines, 1) if HOME_PATH.search(line)]
    if named:
        refuse(f"the {source} names a machine path outside the engine and the corpus:\n"
               + "\n".join(named))
    return lines


def component(gate: Path, roots: list[Path]) -> str:
    """The gate's output as the record keeps it, every line of it tracked."""
    lines = [relative(line, roots) for line in gate.read_text(encoding="utf-8").splitlines()]
    return "\n".join(kept(lines, "gate output")) + "\n"


def verdict(lines: list[str], command: str, shape: str) -> tuple[int, int, re.Match]:
    """Where the make output echoes COMMAND, and the first line after it of
    SHAPE, the verdict that command's tool printed, with its match."""
    opened = next((i for i, line in enumerate(lines) if re.match(command, line)), None)
    if opened is None:
        refuse(f"the make output holds no echo of `{command}`; run every step the Commands section lists")
    for i in range(opened + 1, len(lines)):
        if found := re.fullmatch(shape, lines[i]):
            return opened, i, found
    refuse(f"nothing `{command}` printed in the make output reads as its verdict, `{shape}`")


def build_row(lines: list[str], programs: list[str]) -> str:
    """`make all`'s row: every command carrying the dependency flags' `-MT
    build/...`, which each rule under `all` carries and nothing else the
    steps run does, split into translation units (`-E`) and compile and link
    commands, with every program and its unit among them."""
    commands = [line for line in lines if " -MT build/" in line]
    targets = {re.search(r" -MT (\S+)", line).group(1) for line in commands}
    unbuilt = [target for program in programs for target in (program, program + ".i")
               if target not in targets]
    if unbuilt:
        refuse(f"the make output builds {len(unbuilt)} of the programs' targets nowhere, "
               f"{', '.join(unbuilt[:4])} among them; verify from a clean copy, where make all builds them all")
    units = sum(" -E " in line for line in commands)
    # The standard and warning flags every command carries; `-Wl,` and its
    # kin hold a comma, which a warning flag never does.
    flags = [token for token in commands[0].split()
             if re.fullmatch(r"-(std=\S+|W[\w-]+)", token) and all(token in line.split() for line in commands)]
    return (f"{len(commands)} commands under `{' '.join(flags)}`: {len(commands) - units} compile and "
            f"link commands, every one of the {len(programs)} programs among them, and {units} "
            "translation units")


def consumers_row(lines: list[str], build: Path) -> str:
    """`make check-consumers`'s row: what each Make consumer printed and
    CTest's verdict, from the make output, and what the archive consumer links
    and the installed prefix holds, read from the run's own build/."""
    opened, _, _ = verdict(lines, r"(\S+ )?make -C consumer check", r".*")
    ran, printed = {}, None
    for line in lines[opened + 1:]:
        if found := re.fullmatch(r"env -u METTA_PATH \S+/consumer-make/(\S+)", line):
            printed = found.group(1)
        elif (found := re.fullmatch(r"OK \S+ \((\d+) claims\)", line)) and printed:
            ran[printed], printed = found.group(1), None
        elif re.match(r"(\S+ )?cmake -S consumer", line):
            break
    if not ran:
        refuse("no Make consumer printed OK in the make output")
    _, _, ctest = verdict(lines, r"env -u METTA_PATH ctest ", r"\d+% tests passed, \d+ tests failed out of \d+")
    archive, prefix = build / "consumer-make" / "archive", build / "prefix"
    if not archive.is_file() or not prefix.is_dir():
        refuse("build/ holds no archive consumer or installed prefix; run make check-consumers")
    # The consumers' own check reads the archive's dynamic section the same way.
    dynamic = subprocess.run(["readelf", "-dW", str(archive)], capture_output=True, text=True,
                             check=True).stdout
    entries = sorted(str(path.relative_to(prefix)) for path in prefix.rglob(".git*"))
    return ("; ".join(f"`{name}` prints `OK` ({claims} claims)" for name, claims in ran.items())
            + f"; the archive links {'a' if 'Shared library: [libcmetta' in dynamic else 'no'} `libcmetta.so`"
            + "; CTest with `METTA_PATH` unset: " + ctest.group(0)
            + "; the installed prefix holds "
            + (f"{len(entries)} version-control entr{'y' if len(entries) == 1 else 'ies'}, {', '.join(entries[:3])}"
               if entries else "no `.git*` entry"))


def gate_row(text: str) -> tuple[str, str]:
    """The C seat gate's row, every lane's verdict from the summary with the
    lines GATE_DETAIL quotes from its section, and the cmetta version every
    lane that printed one agrees on."""
    # A summary row is the lane, the status the gate printed, which is `ok`,
    # `FAIL` or `skipped` for a lane that measured nothing, and its seconds
    # [source: the MeTTa checkout's tools/check.sh, its summary table]; the
    # status is quoted as printed, so a lane that measured nothing is named.
    verdicts = re.findall(r"^GATE +(\S+) +(\S+) +\d+s$", text, re.MULTILINE)
    if not verdicts:
        refuse("the gate output holds no summary of GATE lines")
    sections = dict(re.findall(r"^=== (\S+) \[GATE\] ===\n(.*?)(?=^=== |^=+ summary =+$)", text,
                               re.MULTILINE | re.DOTALL))
    versions = set(VERSION.findall(text))
    if len(versions) != 1:
        refuse(f"the gate output names {len(versions)} cmetta versions, {sorted(versions)}; it names one")
    cells = []
    for lane, said in verdicts:
        quoted = []
        for shape in GATE_DETAIL.get(lane, ()):
            for line in sections.get(lane, "").splitlines():
                found = re.match(shape, line)
                if found and found.group(found.re.groups and 1) not in quoted:
                    quoted.append(found.group(found.re.groups and 1))
        cells.append(f"`{lane}` {said}" + (f" ({'; '.join(quoted)})" if quoted else ""))
    return ", ".join(cells), versions.pop()


def results(make: list[str], gate: str, programs: list[str], receipts: dict[str, list],
            build: Path) -> tuple[list[tuple[str, str]], str]:
    """Every Results row, and the cmetta version the gate printed."""
    gate_cell, version = gate_row(gate)
    rows = [("C seat gate", gate_cell)]
    verdict(make, r"ENGINE_PATH=\S+ sh \S+/build\.sh", r".*")
    rows.append(("`make surface`", "the C seat's library built from the engine tree by its `build.sh`"))
    rows.append(("`make all`", build_row(make, programs)))
    for step, command, shape in STEPS:
        opened, at, found = verdict(make, command, shape)
        if step in RECEIPTS and int(found.group(1)) != len(receipts[RECEIPTS[step]]):
            refuse(f"build/{RECEIPTS[step]} holds {len(receipts[RECEIPTS[step]])} entries where the make "
                   f"output's `{found.group(0)}` counts {found.group(1)}; they are not one run's")
        cell = found.group(0)
        if step == "twin lane":
            findings = sum(line.startswith("FINDING ") for line in make[opened + 1:at])
            cell += f"; {findings or 'no'} finding{'s' * (findings != 1)} of the source rule"
        rows.append((step, cell))
    rows.append(("`make check-consumers`", consumers_row(make, build)))
    return rows, version


def table(rows: list[tuple[str, str]]) -> str:
    return "\n".join(["| Step | Result |", "| --- | --- |"]
                     + [f"| {step} | {cell.replace('|', chr(92) + '|')} |" for step, cell in rows])


def identity(root: Path, engine: Path, version: str) -> str:
    """The paragraph naming what ran: the corpus commit, the engine's
    superproject and the three components the corpus reads, each checkout
    refused unless it holds its commit."""
    corpus, superproject = commit_of(root), commit_of(engine)
    stray = [entry for entry in git(root, "status", "--porcelain").splitlines() if entry[3:] not in WRITTEN]
    if stray:
        refuse("the corpus checkout holds more than its commit, so no commit names what ran:\n" + "\n".join(stray))
    moved = git(engine, "status", "--porcelain", "--untracked-files=no", "--ignore-submodules=none")
    if moved:
        refuse("the engine checkout differs from its commit, so no commit names what ran:\n" + moved)
    short = {path: commit_of(engine / path) for path in ("lib", "examples", "extensions/cmetta")}
    return textwrap.fill(
        f"The corpus at `{corpus}` was verified from a checkout "
        "holding that commit's files and nothing else, every program and translation unit built by "
        f"the run. It ran against the MeTTa checkout at superproject "
        f"`{superproject}` with no tracked change, whose library is lib "
        f"`{short['lib']}` and whose originals are examples `{short['examples']}`, pinning "
        f"`extensions/cmetta` at `{short['extensions/cmetta']}`, cmetta {version}. It ran on the host "
        "and toolchain below, which `tools/verification.py` reads from the run's own `swipl`, its "
        "declaration of the host patches it was built with, and the compilers' own version lines:",
        width=78, break_long_words=False, break_on_hyphens=False)


def programs_of(root: Path) -> list[str]:
    """Every program the Makefile discovers, as its build target."""
    listed = subprocess.run(["make", "-s", "--no-print-directory", "-C", str(root), "list"],
                            capture_output=True, text=True, check=True).stdout.split()
    return ["build/" + source.removesuffix(".c") for source in listed]


def receipts_of(root: Path) -> dict[str, list]:
    found = {}
    for name in ("results.json", "twins.json"):
        receipt = root / "build" / name
        if not receipt.is_file():
            refuse(f"build/{name} is missing; run make check first")
        found[name] = json.loads(receipt.read_text(encoding="utf-8"))
    return found


def write(root: Path, engine: Path, gate: Path, make: Path, swipl: str, cc: str) -> None:
    """Every run-determined part of the record, or nothing: each part is read
    and judged before any file is written."""
    roots = [engine.resolve(), root]
    receipts = receipts_of(root)
    rows, version = results([relative(line, roots) for line in make.read_text(encoding="utf-8").splitlines()],
                            gate.read_text(encoding="utf-8"), programs_of(root), receipts,
                            root / "build")
    # Only the lines a cell quotes are kept; the rest of the make output, which
    # names the host's libraries, is read and not kept.
    kept([cell for _, cell in rows], "make output as the Results table quotes it")
    text = (root / "VERIFICATION.md").read_text(encoding="utf-8")
    for name, block in (("identity", identity(root, engine, version)), ("host", host_block(swipl, cc)),
                        ("results", table(rows))):
        text = write_block(text, name, block)
    kept_gate = component(gate, roots)
    (root / "verification").mkdir(exist_ok=True)
    for name in receipts:
        shutil.copy(root / "build" / name, root / "verification" / name)
    (root / "verification" / "component.txt").write_text(kept_gate, encoding="utf-8")
    (root / "VERIFICATION.md").write_text(text, encoding="utf-8")


def check(root: Path) -> list[str]:
    """What keeps the record from shipping: every tracked line naming a
    home-directory path, and a VERIFICATION.md without one of its blocks."""
    # The tracked files as the working tree holds them, read by the pattern
    # the writer refuses with; a file that is not text names no path.
    tracked = git(root, "ls-files", "-z").split("\0")
    wrong = []
    for name in filter(None, tracked):
        try:
            text = (root / name).read_text(encoding="utf-8")
        except (UnicodeDecodeError, OSError):
            continue
        wrong += [f"a tracked line names a path in a home directory: {name}:{number}: {line.strip()[:120]}"
                  for number, line in enumerate(text.splitlines(), 1) if HOME_PATH.search(line)]
    text = (root / "VERIFICATION.md").read_text(encoding="utf-8")
    for begin, end in map(markers, BLOCKS):
        if not 0 <= text.find(begin) < text.find(end):
            wrong.append(f"VERIFICATION.md has no {begin} ... {end} block")
    return wrong


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--engine", type=Path, help="the engine tree the run used")
    parser.add_argument("--gate", type=Path, help="the C seat gate's output from the run")
    parser.add_argument("--make", type=Path, help="the make output of the run's corpus steps")
    parser.add_argument("--swipl", default="swipl")
    parser.add_argument("--cc", default="cc")
    parser.add_argument("--check", action="store_true")
    arguments = parser.parse_args()
    if not arguments.check:
        if None in (arguments.engine, arguments.gate, arguments.make):
            parser.error("writing the record needs --engine, --gate and --make, all from one run")
        write(ROOT, arguments.engine, arguments.gate, arguments.make, arguments.swipl, arguments.cc)
    wrong = check(ROOT)
    for line in wrong:
        print(line, file=sys.stderr)
    return 1 if wrong else 0


if __name__ == "__main__":
    raise SystemExit(main())
