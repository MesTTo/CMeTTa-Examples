"""Purpose: prove the twin lane can come back red. Each case plants a twin of a
real original in a scratch tree, builds it against the corpus's own helpers,
and asks twin_lane.check_one for its verdict, so a lane that stopped looking
at claims, heads, stored content, the engine floor or source text fails here
instead of passing every twin.

Assumes: `make all` has built build/common.o, build/lane.o and
build/tools/original; the engine tree is --engine.
Guarantees: exits nonzero unless every planted defect is reported and the two
planted good twins are not [tested: make check; commit=WORKTREE].
Owns resources: a scratch tree under ai-tmp/, removed on success and kept on
failure for inspection.
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
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
                                          .effect = MT_PURE, .fn = square }));
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
}

SQUARE = """
static mt_status square(mt_call *call, void *user)
{
    (void)user;
    int64_t x = mt_int(mt_arg(call, 0));
    return mt_answer(call, mt_num(x * x));
}
"""


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

    wrong = []
    for name, (original, body, expected) in CASES.items():
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
            wrong.append(f"{name}: did not compile: {build.stderr.strip()[:300]}")
            continue
        verdict = lane.check_one(twin, engine, [])
        said = " | ".join(verdict.findings)
        if expected is None and verdict.findings:
            wrong.append(f"{name}: a good twin was refused: {said}")
        elif expected is not None and expected not in said:
            wrong.append(f"{name}: expected a finding containing {expected!r}, got {said or 'none'}")
        twin.unlink()
    for line in wrong:
        print(f"SELFTEST {line}")
    print(f"{len(CASES) - len(wrong)}/{len(CASES)} planted cases judged as expected")
    if not wrong:
        shutil.rmtree(scratch, ignore_errors=True)
    return 1 if wrong else 0


if __name__ == "__main__":
    sys.exit(main())
