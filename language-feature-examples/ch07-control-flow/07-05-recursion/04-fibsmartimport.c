/* Purpose: using another file's definitions. The original imports
 *   03-fibsmart.metta; C's import is an #include, so this twin includes the
 *   header 03-fibsmart.c is built on and installs the same equations.
 * Guarantees: the original's claim holds [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include "fibsmart.h"

int main(void)
{
    metta *m = open_engine();
    require("the included definitions", install_fibsmart(m));
    check_answers("(fib 100)", mt_eval(m, E("fib", 100)), mt_bigint("354224848179261915075"));
    return done(m);
}
