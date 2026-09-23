/* Purpose: an unbound variable is one. is-var asks the engine what
 *   mt_kind_of asks in C, and the nested if answers the arm C's ?: picks.
 * Guarantees: the original's claim holds [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    mt_atom *a = V("A");
    check_answers("(if (is-var $A) (if True 42 lol) (+ 2 2))",
                  mt_eval(m, E("if", E("is-var", mt_keep(a)), E("if", B(true), 42, "lol"), E("+", 2, 2))),
                  mt_kind_of(a) == MT_VARIABLE ? (true ? N(42) : S("lol")) : N(2 + 2));
    mt_drop(a);
    return done(m);
}
