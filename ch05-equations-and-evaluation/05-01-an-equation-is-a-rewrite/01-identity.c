/* Purpose: an equation is a function. square() is written once, as a macro
 *   body over its operator, so it compiles to a C function and builds the
 *   equation (= (f $x) (* $x $x)) the engine reduces, and the two agree.
 * Guarantees: (f 1) is 1, and the equation computes what the C
 *   function computes over -100..100 [tested 2026-09-27T00:35:58+10:00:
 *   make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>

/* A door the program needs before it can go on: on refusal, say why and stop. */
#define require(what, ok) \
    ((ok) ? (void)0 : (fprintf(stderr, "%s: %s\n", (what), mt_errmsg()), exit(EXIT_FAILURE)))

/* Each MeTTa operator twice, for a body written once over its operators:
   C_X computes in C, and T_X builds the atom (X a b). */
#define C_MUL(a, b) ((a) * (b))
#define T_MUL(a, b) mt_expr("*", a, b)

/* One body, two languages: MUL is C's * in one expansion and the atom
   (* a b) in the other, so the definition cannot say two different things. */
#define SQUARE(MUL, x) MUL(x, x)

static int64_t square(int64_t x) { return SQUARE(C_MUL, x); }

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("define f", mt_add(m, E("=", E("f", V("x")), SQUARE(T_MUL, V("x")))));

    assert(mt_one_int(mt_eval(m, E("f", 1))) == 1 && "(f 1) is 1");

    bool agree = true;
    for (int64_t x = -100; x <= 100 && agree; x++)
        agree = mt_one_int(mt_eval(m, E("f", x))) == square(x);
    assert(agree && "the equation computes square() on -100..100");
    mt_close(m);
    return 0;
}
