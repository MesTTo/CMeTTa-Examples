/* Purpose: sequential bindings. let* is a C block of declarations, each
 *   visible to the ones after it; summed() is that block, and the engine's
 *   let* over the same values must answer what it returns.
 * Guarantees: the original's claim holds [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

static int64_t summed(void)
{
    const int64_t x = 1;
    const int64_t y = 2;
    return x + y;
}

int main(void)
{
    metta *m = open_engine();
    check_answers("let* binds in order",
                  mt_eval(m, E("let*", E(E(V("x"), 1), E(V("y"), 2)), E("+", V("x"), V("y")))), summed());
    return done(m);
}
