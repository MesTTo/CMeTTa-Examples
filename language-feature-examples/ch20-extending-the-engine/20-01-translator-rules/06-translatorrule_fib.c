/* Purpose: a call computed at compile time. fib-tr and fib are chapter 7's
 *   accumulator fib, included from its fibsmart.h, one body that is both a
 *   C function and the equations it lowers. compilefib is a translator rule,
 *   so the (compilefib 10) inside smartfun is expanded and evaluated while
 *   smartfun is compiled, never per call; SMART is smartfun's body over
 *   lowering.h's operators, lowered around that call and compiled in C
 *   around C's own fib(10), and the engine must answer what C computes.
 * Guarantees: the original's claim holds [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include "../../ch07-control-flow/07-05-recursion/fibsmart.h"

#define SMART(MUL, fib10, b) MUL(fib10, b)

int main(void)
{
    metta *m = open_engine();
    require("fib-tr and fib", install_fibsmart(m));
    require("compilefib", mt_add(m, E("=", E("compilefib", V("n")), E("fib", V("n")))));
    require("a rule, so its calls run while compiling",
            mt_one_truth(mt_eval(m, E("add-translator-rule!", "compilefib"))));
    require("smartfun", mt_lower(m, (smartfun $b), SMART(M_MUL, (compilefib 10), $b)));
    const int64_t b = 42;
    check_int("smartfun multiplies by fib 10", mt_one_int(mt_eval(m, E("smartfun", b))), SMART(C_MUL, fib(10), b));
    return done(m);
}
