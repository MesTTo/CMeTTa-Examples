/* Purpose: the accumulator fib, written once in fibsmart.h and run in both
 *   languages. Where int64_t holds the answer the engine must agree with the
 *   C function; fib 100 does not fit, so the engine's unbounded integer
 *   arrives as a BIGINT, which mt_bigint spells by its digits.
 * Guarantees: the original's claim holds, and the C function agrees at 90
 *   [tested: make twins; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include "fibsmart.h"

int main(void)
{
    metta *m = open_engine();
    require("install the accumulator fib", install_fibsmart(m));
    check_answers("C and the engine agree where int64_t holds fib", mt_eval(m, E("fib", 90)), fib(90));
    check_answers("(fib 100) outgrows int64_t", mt_eval(m, E("fib", 100)), mt_bigint("354224848179261915075"));
    return done(m);
}
