"""Purpose: prove the twin lane can come back red. Each case plants a twin of a
real original in a scratch tree, builds it against the corpus's own helpers,
and asks twin_lane.check_one for its verdict, so a lane that stopped looking
at claims, heads, stored content, the engine floor or source text fails here
instead of passing every twin. A space too large to list is judged by its
equations and the hash of everything else, and the cases for that feed the
content rule reports of that shape directly, since no small original is
over the cap.

Assumes: `make all` has built build/common.o, build/lane.o and
build/tools/original; the engine tree is --engine.
Guarantees: exits nonzero unless every planted defect is reported, the two
planted good twins are not, derived() reads a clause as the specializer's
exactly when its own head carries the mark, a real report's equations and
hashes agree with the atoms it lists, and over the enumeration cap the content
rule excuses the equations a C operation carries and nothing else
[tested: make check; commit=WORKTREE].
Owns resources: a scratch tree under ai-tmp/, removed on success and kept on
failure for inspection.
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import twin_lane as lane  # noqa: E402

IDENTITY = "ch05-equations-and-evaluation/05-01-an-equation-is-a-rewrite/01-identity"
REPR = "ch03-atoms-and-expressions/04-repr"

PRELUDE = '#define MT_SHORTHAND\n#include "common.h"\n'

#: name -> (original, C body of main after open_engine, the finding expected
#: or None for a twin the lane must accept)
CASES = {
    "good-lowered": (IDENTITY, """
    require("lower", mt_lower(m, (f $x), (* $x $x)));
    check_int("(f 1)", mt_one_int(mt_eval(m, E("f", 1))), 1);""", None),
    "good-c-operation": (IDENTITY, """
    require("publish", mt_def(m, (mt_op){ .name = "f", .arity = 1,
                                          .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL,
                                          .fn = square }));
    check_int("(f 1)", mt_one_int(mt_eval(m, E("f", 1))), 1);""", None),
    "claims-short": (REPR, """
    check_text("42", mt_show(mt_one(mt_eval(m, E("repr", 42)))), "\\"42\\"");""",
                     "claims and the twin proved"),
    "content-drift": (IDENTITY, """
    require("define", mt_add(m, E("=", E("f", V("x")), E("*", V("x"), 1))));
    check_int("(f 1)", mt_one_int(mt_eval(m, E("f", 1))), 1);""",
                      "stored content differs"),
    "hidden-definition": (IDENTITY, """
    mt_space *elsewhere = mt_space_open(m, "&elsewhere");
    require("define", mt_add(elsewhere, E("=", E("f", V("x")), E("*", V("x"), V("x")))));
    check_int("(f 1)", mt_one_int(mt_eval(elsewhere, E("f", 1))), 1);
    mt_space_close(elsewhere);""", "neither an equation nor a published C operation"),
    "source-text": (IDENTITY, """
    require("define", mt_do(m, "(= (f $x) (* $x $x))"));
    check_int("(f 1)", mt_one_int(mt_eval(m, E("f", 1))), 1);""",
                    "runs MeTTa source through mt_do"),
    "false-claim": (IDENTITY, """
    require("lower", mt_lower(m, (f $x), (* $x $x)));
    check_int("(f 1)", mt_one_int(mt_eval(m, E("f", 1))), 2);""",
                    "the twin failed"),
    "engine-bypass": (REPR, """
    for (int i = 0; i < 6; i++) check("printed", true);
    (void)m;""", "under the floor"),
    "drift-beside-derived": (IDENTITY, """
    require("define", mt_add(m, E("=", E("f", V("x")), E("*", V("x"), 1))));
    require("a clause named as the specializer names its own",
            mt_add(m, E("=", E("f_Spec_k1", V("x")), E("*", V("x"), V("x")))));
    check_int("(f 1)", mt_one_int(mt_eval(m, E("f", 1))), 1);""",
                             "stored content differs"),
}

#: atom -> whether the lane reads it as a clause the specializer derived: only
#: an equation or declaration whose own head carries the mark
DERIVED = {
    "(= (map-flat_Spec_k1 $f $l) (map-flat $f $l))": True,
    "(: f_Spec_k2 (-> Number Number))": True,
    "(= (map-flat $f $l) ($f $l))": False,
    "(= (f $x) (g_Spec_k1 $x))": False,
    "(holds f_Spec_k1)": False,
}

#: A twin holding an equation and a fact, whose report is read for real and
#: held to the atoms it lists.
REPORTED = """
    require("lower", mt_lower(m, (f $x), (* $x $x)));
    require("a fact", mt_add(m, E("color", "red")));
    check_int("(f 2)", mt_one_int(mt_eval(m, E("f", 2))), 4);"""

#: The lowered (= (f $x) (* $x $x)) as lane.c writes it, variables renumbered.
SQUARED = "(= (f $_0) (* $_0 $_0))"

#: name -> (the original's equations, the twin's, the C operations the twin
#: publishes, whether the atoms that are not equations hash alike, and the
#: storage and finding the content rule must give, the finding None where it
#: must raise none). Both sides report a space over the enumeration cap,
#: which lists its equations only, and None for a side's equations is a count
#: larger than the lines it printed.
OVER_CAP = {
    "over-cap-carried": ([SQUARED], [], {"f"}, True, "carried", None),
    "over-cap-data-drift": ([SQUARED], [SQUARED], set(), False, "different",
                            "the atoms that are not equations differ"),
    "over-cap-equation-drift": ([SQUARED], ["(= (f $_0) (* $_0 1))"], set(), True,
                                "different", "stored content differs"),
    "over-cap-uncarried": ([SQUARED], [], set(), True, "different", "stored content differs"),
    "over-cap-unlisted": (None, [SQUARED], set(), True, "different",
                          "the equations are too many to list"),
}

SQUARE = """
static mt_status square(mt_call *call, void *user)
{
    (void)user;
    int64_t x = mt_int(mt_arg(call, 0));
    return mt_answer(call, mt_num(x * x));
}
"""


def fnv1a(line: str) -> int:
    """lane.c's FNV-1a 64 over one line's bytes."""
    h = 14695981039346656037
    for byte in line.encode():
        h = ((h ^ byte) * 1099511628211) % 2**64
    return h


def multiset_hash(lines: list[str]) -> str:
    return f"{sum(map(fnv1a, lines)) % 2**64:016x}"


def plant(name: str, original: str, body: str, cmetta: Path,
          wrong: list[tuple[str, str]]) -> tuple[Path, Path] | None:
    """Write and build a twin of ORIGINAL whose main runs BODY; the source and
    binary, or None with the compiler's complaint added to WRONG."""
    twin = lane.TWINS / f"{original}.c"
    binary = lane.BUILD / "language-feature-examples" / original
    twin.parent.mkdir(parents=True, exist_ok=True)
    binary.parent.mkdir(parents=True, exist_ok=True)
    twin.write_text(PRELUDE + SQUARE + "int main(void)\n{\n    metta *m = open_engine();"
                    + body + "\n    return done(m);\n}\n", encoding="utf-8")
    build = subprocess.run(
        ["cc", "-std=c11", "-Wall", "-Wno-unused-function", "-I", str(lane.ROOT),
         "-I", str(cmetta), str(twin), str(lane.ROOT / "build" / "common.o"),
         str(lane.ROOT / "build" / "lane.o"), f"-L{cmetta}",
         f"-Wl,-rpath,{cmetta}", "-lcmetta", "-lm", "-o", str(binary)],
        capture_output=True, text=True, check=False)
    if build.returncode:
        wrong.append((name, f"did not compile: {build.stderr.strip()[:300]}"))
        twin.unlink()
        return None
    return twin, binary


def report_wrong(run: lane.Run) -> list[str]:
    """How a real report that lists its atoms disagrees with them: its
    equations must be the listed atoms written (= ...), LANE-DATA-HASH the
    multiset hash of the rest and LANE-HASH that of all of them."""
    if not run.ok or run.atoms is None or run.equations is None:
        return [f"the twin failed or listed neither its atoms nor its equations: "
                f"{run.output.strip()[-300:]}"]
    wrong = []
    written = sorted(a for a in run.atoms if a.startswith("(= "))
    if sorted(run.equations) != written:
        wrong.append(f"LANE-EQUATION lines {run.equations} are not the listed equations {written}")
    if SQUARED not in run.equations or "(color red)" not in run.atoms:
        wrong.append(f"the planted equation or fact is missing from {run.atoms}")
    data = list((Counter(run.atoms) - Counter(run.equations)).elements())
    if run.data_hash != multiset_hash(data):
        wrong.append(f"LANE-DATA-HASH {run.data_hash} is not the hash {multiset_hash(data)} "
                     f"of the atoms that are not equations")
    if run.hash != multiset_hash(run.atoms):
        wrong.append(f"LANE-HASH {run.hash} is not the hash {multiset_hash(run.atoms)} "
                     f"of the listed atoms")
    return wrong


def over_cap(equations: list[str] | None, data_hash: str) -> lane.Run:
    """The report of a space over the enumeration cap: no LANE-ATOM lines, its
    equations listed unless too many, and its hash covering both parts."""
    listed = equations or []
    counted = len(listed) if equations is not None else len(listed) + 1
    whole = (int(data_hash, 16) + sum(map(fnv1a, listed))
             + (fnv1a("an equation left unlisted") if equations is None else 0)) % 2**64
    return lane.read(0, "\n".join([
        "LANE-HELD 50001", f"LANE-HASH {whole:016x}",
        *(f"LANE-EQUATION {e}" for e in listed),
        f"LANE-EQUATIONS {counted}", f"LANE-DATA-HASH {data_hash}", "OK"]))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--engine", type=Path, required=True)
    args = parser.parse_args()
    engine = args.engine.resolve()
    cmetta = Path(os.environ.get("CMETTA_DIR", engine / "extensions" / "cmetta"))
    scratch = lane.ROOT / "ai-tmp" / "lane-selftest"
    shutil.rmtree(scratch, ignore_errors=True)
    lane.TWINS = scratch / "language-feature-examples"
    lane.BUILD = scratch / "build"
    lane.RUNNER = lane.ROOT / "build" / "tools" / "original"

    # (case, what it got wrong); one case can get several things wrong.
    wrong = [(atom, f"derived({atom!r}) answered {not want}")
             for atom, want in DERIVED.items() if lane.derived(atom) != want]
    for name, (original, body, expected) in CASES.items():
        planted = plant(name, original, body, cmetta, wrong)
        if planted is None:
            continue
        verdict = lane.check_one(planted[0], engine, [])
        said = " | ".join(verdict.findings)
        if expected is None and verdict.findings:
            wrong.append((name, f"a good twin was refused: {said}"))
        elif expected is not None and expected not in said:
            wrong.append((name, f"expected a finding containing {expected!r}, got {said or 'none'}"))
        planted[0].unlink()

    planted = plant("report", IDENTITY, REPORTED, cmetta, wrong)
    if planted is not None:
        wrong += [("report", w) for w in report_wrong(lane.execute([str(planted[1])], engine))]
        planted[0].unlink()

    for name, (left, right, ops, alike, storage, expected) in OVER_CAP.items():
        got, found = lane.content(over_cap(left, "00000000000000aa"),
                                  over_cap(right, "00000000000000aa" if alike else "00000000000000bb"),
                                  ops, {})
        said = " | ".join(found)
        if got != storage or (expected is None and found) or (expected and expected not in said):
            wrong.append((name, f"expected {storage} with {expected or 'no finding'}, "
                                f"got {got} with {said or 'no finding'}"))

    for case, what in wrong:
        print(f"SELFTEST {case}: {what}")
    judged = len(CASES) + len(DERIVED) + 1 + len(OVER_CAP)
    print(f"{judged - len({case for case, _ in wrong})}/{judged} planted cases judged as expected")
    if not wrong:
        shutil.rmtree(scratch, ignore_errors=True)
    return 1 if wrong else 0


if __name__ == "__main__":
    sys.exit(main())
