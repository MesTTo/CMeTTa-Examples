"""Purpose: prove tools/verification.py can come back red. Each case plants a
record, a gate output, a make output, receipts or checkouts in a scratch
directory and asks the writer or the checker for its verdict, so a writer that
stopped rewriting a root, let a machine path through, quoted the wrong tool's
line, took a receipt from another run or named a checkout that is not at its
commit, or a checker that stopped seeing a path, fails here instead of passing
every record.

Assumes: git, make, cc, readelf and `swipl` on PATH, as `make check` has them.
Guarantees: exits nonzero unless every planted case is judged as expected
[tested 2026-09-26T03:22:18+10:00: twelve mutants of tools/verification.py,
one at a time, each disabling one of the receipt cross-check, the self-tests'
attribution by their echo, the unbuilt-target check, the corpus checkout
check, the engine checkout check, judging before writing, the one-version
rule, the FINDING count, relative(), HOME_PATH, reading a `skipped` gate
lane and refusing a tree that is not a checkout of its own, read between
33/38 and 37/38 and exited 1; intact it reads 38/38].
Owns resources: a scratch directory under ai-tmp/, removed on success and
kept on failure for inspection.
"""

from __future__ import annotations

import json
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
WINDOWS = "C:" + "\\" + "Users" + "\\" + "someone"


def refused(action) -> bool:
    try:
        action()
    except SystemExit as stop:
        return stop.code not in (None, 0)
    return False


def scratch() -> Path:
    place = SCRATCH / str(len(list(SCRATCH.glob("*"))) if SCRATCH.exists() else 0)
    place.mkdir(parents=True)
    return place


def planted(files: dict[str, str], where: Path | None = None, commit: bool = False) -> Path:
    """A scratch repository tracking exactly these files, committed if asked."""
    repo = where or scratch()
    for name, text in files.items():
        (repo / name).parent.mkdir(parents=True, exist_ok=True)
        (repo / name).write_text(text, encoding="utf-8")
    subprocess.run(["git", "init", "-q", str(repo)], check=True)
    subprocess.run(["git", "-C", str(repo), "add", "-A"], check=True)
    if commit:
        subprocess.run(["git", "-C", str(repo), "-c", "user.name=selftest", "-c", "user.email=selftest@invalid",
                        "commit", "-q", "-m", "planted"], check=True)
    return repo


def gate(lines: list[str]) -> Path:
    return planted({"gate.log": "\n".join(lines) + "\n"}) / "gate.log"


BLOCK = "".join(f"{begin}\n{end}\n" for begin, end in map(record.markers, record.BLOCKS))
FLAGS = "-O2 -g -std=c11 -Wall -Wextra -Wpedantic -Werror"
PROGRAMS = ["build/basics/a", "build/data/b"]


def compiled(target: str, unit: bool) -> str:
    source = target.removeprefix("build/").removesuffix(".i").removesuffix(".o") + ".c"
    if unit:
        return f"cc -E -MMD -MP -MF {target}.d -MT {target} -I. {FLAGS} {source} -o {target}"
    return (f"cc -MMD -MP -MF {target}.d -MT {target} -I. {FLAGS} {source} build/common.o "
            f"-Wl,-rpath,x -lcmetta -o {target}")


def make_output(programs: list[str] = PROGRAMS, drop: str = "", findings: int = 0) -> list[str]:
    """A make output of the four steps as make echoes them, less any line
    holding DROP, with FINDINGS lines from the twin lane."""
    lines = (["ENGINE_PATH=. sh extensions/cmetta/build.sh", compiled("build/common.o", False)]
             + [compiled(p + ".i", True) for p in programs] + [compiled(p, False) for p in programs]
             + ["python3 tools/run.py --jobs 4 build/basics/a", "2/2 examples passed",
                "python3 tools/twin_lane.py --jobs 4 --engine ."]
             + [f"FINDING language-feature-examples/x.c: finding {n}" for n in range(findings)]
             + ["3/3 twins agree with their originals; 5/5 claims proved; stored content equal 3",
                "python3 tools/twin_lane_selftest.py --engine .", "26/26 planted cases judged as expected",
                "python3 tools/index.py --engine . --check", "index, coverage and README verified",
                "python3 tools/verification_selftest.py", "13/13 planted cases judged as expected",
                "python3 tools/verification.py --check",
                "make -C extensions/cmetta install ENGINE_PATH=. PREFIX=build/prefix",
                "PKG_CONFIG_PATH=build/prefix/lib/pkgconfig make -C consumer check",
                "env -u METTA_PATH build/consumer-make/shared", "OK integration/installed_consumer.c (2 claims)",
                "env -u METTA_PATH build/consumer-make/archive", "OK integration/installed_consumer.c (2 claims)",
                "PKG_CONFIG_PATH=build/prefix/lib/pkgconfig cmake -S consumer -B build/consumer-cmake",
                "env -u METTA_PATH ctest --test-dir build/consumer-cmake --output-on-failure",
                "100% tests passed, 0 tests failed out of 1"])
    return [line for line in lines if not drop or drop not in line]


GATE = """=== c-binding [GATE] ===
surface: 3 declarations, all defined
version: 2.0.0, and mt_version() agrees
7 checks, 0 failures
=== c-bench [GATE] ===
NOT MEASURED IN THIS CONFIGURATION boot: estimated cycles not compared; checkout length 65, depth 9; \
canonical length 29, depth 5; more
x: x instruction regression: minimum of [5, 6, 7] is 5, baseline 4 plus 0.3% is 4
1 comparison(s) declined for the reasons above
=== c-install [GATE] ===
version: 2.0.0, and mt_version() agrees
================ summary ================
GATE   c-binding    ok  1s
GATE   c-bench      FAIL 1s
GATE   c-install    ok  1s
GATE   c-sanitize   skipped 0s

GATE FAILED: c-bench
"""
GATE_CELL = ("`c-binding` ok (surface: 3 declarations, all defined; 7 checks, 0 failures), `c-bench` FAIL "
             "(x instruction regression: minimum of [5, 6, 7] is 5, baseline 4 plus 0.3% is 4; "
             "1 comparison(s) declined; checkout length 65, depth 9; canonical length 29, depth 5), `c-install` ok, "
             "`c-sanitize` skipped")
RECEIPTS = {"results.json": [{}, {}], "twins.json": [{}, {}, {}]}


def build(linked: bool = False, git_entry: bool = False) -> Path:
    """A build directory holding an archive consumer, linking a planted
    libcmetta when LINKED, and an installed prefix."""
    place = scratch() / "build"
    (place / "consumer-make").mkdir(parents=True)
    (place / "prefix" / "share").mkdir(parents=True)
    if git_entry:
        (place / "prefix" / "share" / ".git").write_text("gitdir: elsewhere\n", encoding="utf-8")
    archive = place / "consumer-make" / "archive"
    if linked:
        (place / "lib.c").write_text("int planted(void) { return 0; }\n", encoding="utf-8")
        (place / "main.c").write_text("int planted(void);\nint main(void) { return planted(); }\n", encoding="utf-8")
        subprocess.run(["cc", "-shared", "-fPIC", str(place / "lib.c"), "-o", str(place / "libcmetta.so")], check=True)
        subprocess.run(["cc", str(place / "main.c"), f"-L{place}", "-Wl,--no-as-needed", "-lcmetta",
                        "-o", str(archive)], check=True)
    else:
        shutil.copy(shutil.which("true"), archive)
    return place


def rows(**change) -> dict[str, str]:
    arguments = {"make": make_output(), "gate": GATE, "programs": PROGRAMS, "receipts": RECEIPTS,
                 "build": build()} | change
    return dict(record.results(**arguments)[0])


def engine(dirty: bool = False) -> Path:
    """A committed engine checkout holding lib, examples and the C seat as
    committed repositories of their own."""
    root = planted({"README.md": "engine\n"}, commit=True)
    for path in ("lib", "examples", "extensions/cmetta"):
        planted({"README.md": f"{path}\n"}, root / path, commit=True)
    if dirty:
        (root / "README.md").write_text("changed\n", encoding="utf-8")
    return root


def corpus(extra: dict[str, str] | None = None, receipts: dict | None = None) -> Path:
    """A committed corpus with a record and a Makefile listing two programs,
    then EXTRA written over it, and build/ holding the receipts and consumers."""
    makefile = "list:\n\t@printf '%s\\n' basics/a.c data/b.c\n"
    root = planted({"VERIFICATION.md": f"# Verification\n\n{BLOCK}\n", "Makefile": makefile,
                    ".gitignore": "/build/\n"}, commit=True)
    for name, text in (extra or {}).items():
        (root / name).write_text(text, encoding="utf-8")
    shutil.copytree(build(), root / "build")
    for name, entries in (receipts or RECEIPTS).items():
        (root / "build" / name).write_text(json.dumps(entries), encoding="utf-8")
    return root


def written(root: Path, gate_lines: str = GATE, make_lines: list[str] | None = None) -> str:
    """The record after the writer ran on the planted run."""
    run = scratch()
    (run / "gate.log").write_text(gate_lines, encoding="utf-8")
    (run / "make.log").write_text("\n".join(make_lines or make_output()) + "\n", encoding="utf-8")
    record.write(root, engine(), run / "gate.log", run / "make.log", "swipl", "cc")
    return (root / "VERIFICATION.md").read_text(encoding="utf-8")


def host_named_by_identity() -> bool:
    block = record.host_block("swipl", "cc")
    return block.startswith("- SWI-Prolog ") and not record.HOME_PATH.search(block)


def linked_archive_and_git_entry_named() -> bool:
    cell = rows(build=build(linked=True, git_entry=True))["`make check-consumers`"]
    return "the archive links a `libcmetta.so`" in cell and "1 version-control entry, share/.git" in cell


def writer_fills_every_block() -> bool:
    root = corpus()
    text = written(root)
    parts = (f"The corpus at `{record.git(root, 'rev-parse', '--short', 'HEAD')}`", "cmetta 2.0.0",
             "- SWI-Prolog ", "| twin lane | 3/3 twins agree")
    return all(part in text for part in parts) and (root / "verification" / "twins.json").is_file()


def identity_names_every_commit() -> bool:
    root, tree = corpus(), engine()
    paragraph = " ".join(record.identity(root, tree, "2.0.0").split())
    return all(record.git(where, "rev-parse", "--short", "HEAD") in paragraph
               for where in (root, tree, tree / "lib", tree / "examples", tree / "extensions/cmetta"))


def refused_run_writes_nothing() -> bool:
    root = corpus()
    return (refused(lambda: written(root, make_lines=make_output(drop="index, coverage")))
            and (root / "VERIFICATION.md").read_text(encoding="utf-8") == f"# Verification\n\n{BLOCK}\n"
            and not (root / "verification").exists())


def archived_engine_refused() -> bool:
    """A tree written out of a checkout, as `git archive` writes one, inside
    another repository, where git would read the outer repository's commit."""
    tree = planted({"README.md": "outer\n"}, commit=True) / "release"
    for path in ("lib", "examples", "extensions/cmetta"):
        (tree / path).mkdir(parents=True)
    return refused(lambda: record.identity(corpus(), tree, "2.0.0"))


def plain_component_refused() -> bool:
    """An engine checkout whose lib is a plain directory of its own files."""
    tree = planted({"README.md": "engine\n", "lib/README.md": "lib\n"}, commit=True)
    for path in ("examples", "extensions/cmetta"):
        planted({"README.md": f"{path}\n"}, tree / path, commit=True)
    return refused(lambda: record.identity(corpus(), tree, "2.0.0"))


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
    "a Windows checkout root is refused too":
        lambda: refused(lambda: record.component(gate([f"under {WINDOWS}\\prefix"]), [ENGINE, ROOT])),
    "a URI scheme's tail is not read as a drive":
        lambda: record.component(gate(["see " + "foo:" + "/a" + "//b"]), [ENGINE, ROOT]) == "see foo:/a//b\n",
    "a gate output inside the roots is kept":
        lambda: record.component(gate([f"under {ENGINE}/extensions/cmetta/build", "GATE c-binding ok"]),
                                 [ENGINE, ROOT]) == "under extensions/cmetta/build\nGATE c-binding ok\n",
    "a quoted cell naming a home-directory path is refused":
        lambda: refused(lambda: record.kept([f"OK {HOMES[0] / 'someone' / 'x.c'} (2 claims)"], "make output")),
    "the checker names a tracked line with a home-directory path":
        lambda: record.check(planted({"VERIFICATION.md": BLOCK,
                                      "notes.txt": f"the host at {HOMES[0] / 'me' / 'swipl' / 'bin'}\n"})) != [],
    "the checker passes a record with none":
        lambda: record.check(planted({"VERIFICATION.md": BLOCK, "notes.txt": "the host, SWI-Prolog 10.1.14\n"})) == [],
    "the checker names a record without its host block":
        lambda: record.check(planted({"VERIFICATION.md": "no block here\n"})) != [],
    "the checker names a record without its results block":
        lambda: record.check(planted({"VERIFICATION.md": BLOCK.replace("results", "other")})) != [],
    "the host block names the host by identity and no path":
        host_named_by_identity,
    "a block is replaced between its markers and nowhere else":
        lambda: record.write_block("a\n" + BLOCK + "b\n", "results", "new")
        == "a\n" + BLOCK.replace("<!-- results:begin -->\n", "<!-- results:begin -->\nnew\n") + "b\n",
    "each step's cell is the verdict line its own tool printed":
        lambda: {step: rows()[step] for step in ("embedding programs", "index")}
        == {"embedding programs": "2/2 examples passed", "index": "index, coverage and README verified"},
    "the two self-tests are told apart by the command that printed them":
        lambda: (rows()["lane self-test"], rows()["record self-test"])
        == ("26/26 planted cases judged as expected", "13/13 planted cases judged as expected"),
    "the twin lane's cell counts its FINDING lines":
        lambda: (rows()["twin lane"].endswith("; no findings of the source rule")
                 and rows(make=make_output(findings=1))["twin lane"].endswith("; 1 finding of the source rule")),
    "make all's cell counts commands, units and the flags every command carries":
        lambda: rows()["`make all`"] == "5 commands under `-std=c11 -Wall -Wextra -Wpedantic -Werror`: 3 compile "
        "and link commands, every one of the 2 programs among them, and 2 translation units",
    "a program the make output never built is refused":
        lambda: refused(lambda: rows(programs=PROGRAMS + ["build/data/c"])),
    "a translation unit the make output never wrote is refused":
        lambda: refused(lambda: rows(make=make_output(drop="-MT build/data/b.i"))),
    "a step the make output never ran is refused":
        lambda: refused(lambda: rows(make=make_output(drop="tools/index.py"))),
    "a step whose verdict line is missing is refused":
        lambda: refused(lambda: rows(make=make_output(drop="index, coverage"))),
    "a receipt holding another run's count is refused":
        lambda: refused(lambda: rows(receipts=RECEIPTS | {"twins.json": [{}] * 6})),
    "the gate's cell quotes each lane's verdict and its details, failing rows included":
        lambda: rows()["C seat gate"] == GATE_CELL,
    "two cmetta versions in the gate output are refused":
        lambda: refused(lambda: rows(gate=GATE.replace("version: 2.0.0", "version: 1.0.0", 1))),
    "a gate output without its summary is refused":
        lambda: refused(lambda: rows(gate=GATE.split("====")[0])),
    "the consumers' cell names each consumer, CTest and a clean prefix":
        lambda: rows()["`make check-consumers`"] == "`shared` prints `OK` (2 claims); `archive` prints `OK` "
        "(2 claims); the archive links no `libcmetta.so`; CTest with `METTA_PATH` unset: 100% tests passed, "
        "0 tests failed out of 1; the installed prefix holds no `.git*` entry",
    "an archive linking libcmetta and a .git entry in the prefix are named":
        linked_archive_and_git_entry_named,
    "the writer fills all three blocks and copies the receipts": writer_fills_every_block,
    "the writer accepts a checkout whose only changes are its own files":
        lambda: "cmetta 2.0.0" in written(corpus({"VERIFICATION.md": f"# changed\n\n{BLOCK}\n"})),
    "a corpus checkout holding an untracked file is refused":
        lambda: refused(lambda: written(corpus({"stray.c": "int x;\n"}))),
    "an engine checkout with a tracked change is refused":
        lambda: refused(lambda: record.identity(corpus(), engine(dirty=True), "2.0.0")),
    "an engine tree that is not a checkout of its own is refused": archived_engine_refused,
    "a component that is not a checkout of its own is refused": plain_component_refused,
    "the identity names the corpus, the superproject and the three components it reads":
        identity_names_every_commit,
    "a refused run writes nothing": refused_run_writes_nothing,
}


def main() -> int:
    shutil.rmtree(SCRATCH, ignore_errors=True)
    wrong = []
    for name, judged in CASES.items():
        try:
            if not judged():
                wrong.append(name)
        # A case that raises, or that the writer refuses where no refusal was
        # planted, was not judged as expected, and the other cases still run.
        except (Exception, SystemExit) as error:
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
