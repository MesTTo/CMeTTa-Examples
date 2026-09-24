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
    require("TupleConcat", mt_lower(m, (TupleConcat $Ev1 $Ev2),
                                    (collapse (superpose ((superpose $Ev1) (superpose $Ev2))))));
    require("range", mt_lower(m, (range $K $N),
                              (if (< $K $N) (TupleConcat ($K) (range (+ $K 1) $N)) ())));

    mt_atom *counted[9];
    for (int64_t i = 0; i < 9; i++) counted[i] = N(i + 1);
    check_answers("(range 1 10)", mt_eval(m, E("range", 1, 10)), mt_exprv(9, counted));
    return done(m);
}
