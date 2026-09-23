/* Purpose: specialization through a cycle. f1 and f2 call each other with
 *   their function argument, as do f3 and f4, and the specializer follows the
 *   cycle without looping; the branch that would call the function never
 *   runs.
 * Guarantees: (f1 + 2) and (f3 + 1) both answer finish [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    mt_atom *never = E(V("f"), "nevercalled", 42);           /* ($f nevercalled 42) */
    /* (= (f1 $f $a) (if (< $a 0) ($f nevercalled 42) (if (== $a 0) (f2 $f (- $a 1)) finish))) */
    require("define f1", mt_add(m, E("=", E("f1", V("f"), V("a")),
        E("if", E("<", V("a"), 0), mt_keep(never),
          E("if", E("==", V("a"), 0), E("f2", V("f"), E("-", V("a"), 1)), "finish")))));
    /* (= (f2 $f $a) (if (< $a 0) ($f nevercalled 42) (f1 $f $a))) */
    require("define f2", mt_add(m, E("=", E("f2", V("f"), V("a")),
        E("if", E("<", V("a"), 0), never, E("f1", V("f"), V("a"))))));
    check_answers("the first cycle", mt_eval(m, E("f1", "+", 2)), "finish");

    /* (= (f3 $f $n) (if (== $n 0) finish (f4 $f $n))) and (= (f4 $f $n) (f3 $f (- $n 1))) */
    require("define f3", mt_add(m, E("=", E("f3", V("f"), V("n")),
                                     E("if", E("==", V("n"), 0), "finish", E("f4", V("f"), V("n"))))));
    require("define f4", mt_add(m, E("=", E("f4", V("f"), V("n")), E("f3", V("f"), E("-", V("n"), 1)))));
    check_answers("the second cycle", mt_eval(m, E("f3", "+", 1)), "finish");
    return done(m);
}
