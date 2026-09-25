/* Purpose: fib by iteration. fib-step turns the pair (a b) into
 *   (b a+b), and iterate carries the pair n times; C carries the same pair
 *   in two variables. Where int64_t holds the answer the engine must agree
 *   with C; fib 100 does not fit, and arrives as a BIGINT.
 * Guarantees: the original's claim holds, and C agrees at 90 [tested: make
 *   twins; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

static int64_t fib(int64_t n)
{
    int64_t a = 0, b = 1;
    for (int64_t i = 0; i < n; i++) {
        int64_t next = a + b;
        a = b;
        b = next;
    }
    return a;
}

int main(void)
{
    metta *m = open_engine();
    require("import lib_patrick", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_patrick")))));
    require("fib-step", mt_add(m, E("=", E("fib-step", V("i"), E(V("a"), V("b"))), E(V("b"), E("+", V("a"), V("b"))))));
    require("fib", mt_add(m, E("=", E("fib", V("n")), E("first", E("iterate", 0, V("n"), E(0, 1), "fib-step")))));

    check_answers("C and the engine agree where int64_t holds fib", mt_eval(m, E("fib", 90)), fib(90));
    check_answers("(fib 100)", mt_eval(m, E("fib", 100)), mt_bigint("354224848179261915075"));
    return done(m);
}
