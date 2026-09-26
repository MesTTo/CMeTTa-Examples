/* Purpose: two Prolog goals run from MeTTa in sequence, the second reading
 *   what the first bound. translatePredicate runs a Prolog goal written as a
 *   term: (is $x 2) binds x to C's 2, and (+ $x 40 $z) is the engine's
 *   three-place +, inputs first and output last, so z is what C's own
 *   addition gives.
 * Guarantees: the original's claim holds [tested 2026-09-27T00:35:58+10:00:
 *   make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>

/* Each MeTTa operator twice, for a body written once over its operators:
   C_X computes in C, and T_X builds the atom (X a b). */
#define C_ADD(a, b) ((a) + (b))

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    const int64_t x = 2, addend = 40;
    assert(mt_one_int(mt_eval(m, E("progn", E("translatePredicate", E("is", V("x"), x)),
                                   E("translatePredicate", E("+", V("x"), addend, V("z"))), V("z")))) == C_ADD(x, addend)
           && "the second goal reads what the first bound");
    mt_close(m);
    return 0;
}
