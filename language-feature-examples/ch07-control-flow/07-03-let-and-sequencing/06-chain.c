/* Purpose: chain names its result. It is C's assignment in sequence: scaled
 *   and summed name each intermediate value once, and the engine's chains
 *   over the same arithmetic must answer what they return.
 * Guarantees: both claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

static int64_t scaled(void)
{
    const int64_t n = 2 + 4;
    return 3 * n;
}

static int64_t summed(void)
{
    const int64_t n = 1 + 3;
    const int64_t doubled = 2 * n;
    return n + doubled;
}

int main(void)
{
    metta *m = open_engine();
    check_answers("one name", mt_eval(m, E("chain", E("+", 2, 4), V("n"), E("*", 3, V("n")))), scaled());
    check_answers("two names",
                  mt_eval(m, E("chain", E("+", 1, 3), V("n"),
                               E("chain", E("*", 2, V("n")), V("m"), E("+", V("n"), V("m"))))), summed());
    return done(m);
}
