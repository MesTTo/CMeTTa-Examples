/* Purpose: an equation is an atom, so removing the atom removes the
 *   definition. f and g are the same equation over different names; mt_del()
 *   takes f's out of &self and (f 3 4) is left with nothing to reduce it,
 *   while (g 3 4) still computes. A plain fact comes and goes the same way.
 * Guarantees: (f 3 4) answers itself, (g 3 4) is 7, and (my test) is gone
 *   [tested: make twins; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

/* (= (name $x $y) (+ $x $y)) */
static mt_atom *sum_equation(const char *name)
{
    return E("=", E(name, V("x"), V("y")), E("+", V("x"), V("y")));
}

int main(void)
{
    metta *m = open_engine();
    require("define f", mt_add(m, sum_equation("f")));
    require("define g", mt_add(m, sum_equation("g")));
    require("remove f's equation", mt_del(m, sum_equation("f")));

    check_answers("(f 3 4) has nothing left to reduce it", mt_eval(m, E("f", 3, 4)), E("f", 3, 4));
    check_int("(g 3 4) is 7", mt_one_int(mt_eval(m, E("g", 3, 4))), 7);

    require("store (my test)", mt_add(m, E("my", "test")));
    require("remove it", mt_del(m, E("my", "test")));
    check_none("(my test) is gone", mt_match(m, E("my", "test")));
    return done(m);
}
