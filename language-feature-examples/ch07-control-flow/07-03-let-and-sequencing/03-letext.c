/* Purpose: let matches a pattern, binding on both sides at once: $x from the
 *   value and $z from the pattern. mt_solve runs that let and reads both by
 *   name; the sum of what was bound is evaluated after.
 * Guarantees: the original's claim holds [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    mt_atom *x = NULL, *z = NULL;
    mt_rows (row, mt_solve(m, E(V("x"), E(42, E("if", E("==", V("x"), 2), 43, 44))), E(3, E(42, V("z"))))) {
        mt_drop(x);
        mt_drop(z);
        x = mt_keep(mt_bound(row, "x"));
        z = mt_keep(mt_bound(row, "z"));
    }
    require("both sides bound", x && z);
    check_answers("the sum of what was bound", mt_eval(m, E("+", x, z)), 47);
    return done(m);
}
