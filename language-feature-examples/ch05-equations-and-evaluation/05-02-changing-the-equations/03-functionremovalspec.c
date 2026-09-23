/* Purpose: removing an equation from a specialized function. (f g) makes the
 *   engine specialize f on g; taking one of f's equations out afterwards
 *   still leaves the specialized call answering from the one that remains,
 *   and putting it back adds its answer after the other's.
 * Guarantees: (f g) answers 2 and 3, then 3, then 3 and 2 [tested: make
 *   twins; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    require("(= (g $x) (+ $x 1))", mt_add(m, E("=", E("g", V("x")), E("+", V("x"), 1))));
    mt_atom *first = E("=", E("f", V("g")), E(V("g"), 1));      /* (= (f $g) ($g 1)) */
    require("(= (f $g) ($g 1))", mt_add(m, mt_keep(first)));
    require("(= (f $g) ($g 2))", mt_add(m, E("=", E("f", V("g")), E(V("g"), 2))));
    check_answers("both equations answer", mt_eval(m, E("f", "g")), 2, 3);

    require("take the first out", mt_del(m, mt_keep(first)));
    check_answers("the specialized call answers from the one left", mt_eval(m, E("f", "g")), 3);
    require("put it back", mt_add(m, first));
    check_answers("and it answers after the other now", mt_eval(m, E("f", "g")), 3, 2);
    return done(m);
}
