/* Purpose: a compile-time regression, kept as a program. evolve wraps its
 *   function argument in twice on every recursion, which once made each
 *   nested specialization build a larger key and never finish compiling;
 *   re-specializing a function already being specialized is now refused.
 *   The three equations are built as terms because the compiler is what is
 *   under test.
 * Guarantees: (evolve derive 2 stmt) terminates and answers stmt [tested:
 *   make twins; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    require("(= (derive $g) $g)", mt_add(m, E("=", E("derive", V("g")), V("g"))));
    require("(= (twice $r $g) ($r ($r $g)))",
            mt_add(m, E("=", E("twice", V("r"), V("g")), E(V("r"), E(V("r"), V("g"))))));
    /* (= (evolve $r $n $g) (if (== $n 0) $g (evolve (twice $r) (- $n 1) $g))) */
    require("define evolve", mt_add(m, E("=", E("evolve", V("r"), V("n"), V("g")),
        E("if", E("==", V("n"), 0), V("g"), E("evolve", E("twice", V("r")), E("-", V("n"), 1), V("g"))))));
    check_answers("evolving twice terminates", mt_eval(m, E("evolve", "derive", 2, "stmt")), "stmt");
    return done(m);
}
