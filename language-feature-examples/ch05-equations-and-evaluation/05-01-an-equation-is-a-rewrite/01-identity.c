/* Purpose: an equation is a function. square() is written once, as a macro
 *   body over its operator, so it compiles to a C function and lowers to the
 *   equation (= (f $x) (* $x $x)) the engine reduces, and the two agree.
 * Guarantees: (f 1) is 1, and the lowered equation computes what the C
 *   function computes over -100..100 [tested: make twins; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include "lowering.h"

/* One body, two languages: MUL is C's * in one expansion and MeTTa's (* a b)
   in the other, so the definition cannot say two different things. */
#define SQUARE(MUL, x) MUL(x, x)

static int64_t square(int64_t x) { return SQUARE(C_MUL, x); }

int main(void)
{
    metta *m = open_engine();
    require("lower f", mt_lower(m, (f $x), SQUARE(M_MUL, $x)));

    check_int("(f 1) is 1", mt_one_int(mt_eval(m, E("f", 1))), 1);

    bool agree = true;
    for (int64_t x = -100; x <= 100 && agree; x++)
        agree = mt_one_int(mt_eval(m, E("f", x))) == square(x);
    check("the equation computes square() on -100..100", agree);
    return done(m);
}
