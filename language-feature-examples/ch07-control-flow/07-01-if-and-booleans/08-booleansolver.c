/* Purpose: solving for a boolean. and and or are relational, so the engine
 *   answers every pair of values satisfying (and (or $x True) $y); C walks
 *   the same two-valued space with two loops, True before False as the
 *   engine tries them, and keeps the pairs its own && and || accept.
 * Guarantees: the original's claim holds [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    mt_atom *satisfying[4];
    size_t n = 0;
    for (int x = 1; x >= 0; x--)
        for (int y = 1; y >= 0; y--)
            if ((x || true) && y) satisfying[n++] = E(B(x), B(y));
    check_list_("the pairs that satisfy the condition",
                mt_all(mt_eval(m, E("if", E("and", E("or", V("x"), B(true)), V("y")), E(V("x"), V("y"))))),
                n, satisfying);
    return done(m);
}
