/* Purpose: integer division written in minimal MeTTa's instructions and run
 *   70,000 steps deep. div subtracts the divisor, compares the rest with 0
 *   through unify, and counts, each step a chain over an eval; C builds it
 *   as the term it is. It counts how many times the divisor comes out of the
 *   dividend before the rest goes negative, which is C's integer division
 *   for a non-negative dividend and a positive divisor, and the engine must
 *   answer that under the stack budget the original raises for the one
 *   evaluation.
 * Guarantees: the original's claim holds [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"
#include "lowering.h"

int main(void)
{
    metta *m = open_engine();
    require("import lib_he", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_he")))));
    require("div",
            mt_add(m, E("=", E("div", V("x"), V("y"), V("accum")),
                        E("chain", E("eval", T_SUB(V("x"), V("y"))), V("r1"),
                          E("chain", E("eval", T_LT(V("r1"), 0)), V("r2"),
                            E("chain",
                              E("unify", V("r2"), B(true), V("accum"),
                                E("chain", E("eval", T_ADD(1, V("accum"))), V("inc"),
                                  E("chain", E("eval", E("div", V("r1"), V("y"), V("inc"))), V("r4"), V("r4")))),
                              V("r3"), V("r3")))))));
    const int64_t dividend = 350000, divisor = 5, depth = 1000000;
    check_int("div counts what C's division counts",
              mt_one_int(mt_eval(m, E("with-pragma!", E(E("max-stack-depth", depth)),
                                      E("chain", E("eval", E("div", dividend, divisor, 0)), V("rr"), V("rr"))))),
              dividend / divisor);
    return done(m);
}
