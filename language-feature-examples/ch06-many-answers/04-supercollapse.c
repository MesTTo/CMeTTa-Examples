/* Purpose: appending through answer sets. TupleConcat superposes the members
 *   of two expressions and collapses them back into one, and range counts by
 *   concatenating a one-member expression onto the rest; the nine members C
 *   expects are a loop.
 * Guarantees: the original's claim holds [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    require("TupleConcat", mt_add(m, E("=", E("TupleConcat", V("Ev1"), V("Ev2")),
                                      E("collapse", E("superpose", E(E("superpose", V("Ev1")), E("superpose", V("Ev2"))))))));
    require("range", mt_add(m, E("=", E("range", V("K"), V("N")),
                                E("if", E("<", V("K"), V("N")),
                                  E("TupleConcat", E(V("K")), E("range", E("+", V("K"), 1), V("N"))), mt_unit()))));

    mt_atom *counted[9];
    for (int64_t i = 0; i < 9; i++) counted[i] = N(i + 1);
    check_answers("(range 1 10)", mt_eval(m, E("range", 1, 10)), mt_exprv(9, counted));
    return done(m);
}
