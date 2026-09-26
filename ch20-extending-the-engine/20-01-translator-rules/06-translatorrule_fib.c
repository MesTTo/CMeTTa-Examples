/* Purpose: a call computed at compile time. fib-tr and fib are chapter 7's
 *   accumulator fib, included from its fibsmart.h, one body that is both a
 *   C function and the equations it builds. compilefib is a translator rule,
 *   so the (compilefib 10) inside smartfun is expanded and evaluated while
 *   smartfun is compiled, never per call; SMART is smartfun's body over
 *   the C_ and T_ operators, built around that call as an atom and compiled
 *   in C around C's own fib(10), and the engine must answer what C computes.
 * Guarantees: the original's claim holds [tested 2026-09-27T00:35:58+10:00:
 *   make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include "../../ch07-control-flow/07-05-recursion/_fixtures/fibsmart.h"

/* A door the program needs before it can go on: on refusal, say why and stop. */
#define require(what, ok) \
    ((ok) ? (void)0 : (fprintf(stderr, "%s: %s\n", (what), mt_errmsg()), exit(EXIT_FAILURE)))

/* Each MeTTa operator twice, for a body written once over its operators:
   C_X computes in C, and T_X builds the atom (X a b). */
#define C_MUL(a, b) ((a) * (b))
#define T_MUL(a, b) mt_expr("*", a, b)

#define SMART(MUL, fib10, b) MUL(fib10, b)

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("fib-tr and fib", install_fibsmart(m));
    require("compilefib", mt_add(m, E("=", E("compilefib", V("n")), E("fib", V("n")))));
    require("a rule, so its calls run while compiling",
            mt_one_truth(mt_eval(m, E("add-translator-rule!", "compilefib"))));
    require("smartfun", mt_add(m, E("=", E("smartfun", V("b")), SMART(T_MUL, E("compilefib", 10), V("b")))));
    const int64_t b = 42;
    assert(mt_one_int(mt_eval(m, E("smartfun", b))) == SMART(C_MUL, fib(10), b) && "smartfun multiplies by fib 10");
    mt_close(m);
    return 0;
}
