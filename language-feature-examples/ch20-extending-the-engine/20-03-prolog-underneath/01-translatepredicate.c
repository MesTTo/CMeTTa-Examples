/* Purpose: two Prolog goals run from MeTTa in sequence, the second reading
 *   what the first bound. translatePredicate runs a Prolog goal written as a
 *   term: (is $x 2) binds x to C's 2, and (+ $x 40 $z) is the engine's
 *   three-place +, inputs first and output last, so z is what C's own
 *   addition gives.
 * Guarantees: the original's claim holds [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include "lowering.h"

int main(void)
{
    metta *m = open_engine();
    const int64_t x = 2, addend = 40;
    check_int("the second goal reads what the first bound",
              mt_one_int(mt_eval(m, E("progn", E("translatePredicate", E("is", V("x"), x)),
                                      E("translatePredicate", E("+", V("x"), addend, V("z"))), V("z")))),
              C_ADD(x, addend));
    return done(m);
}
