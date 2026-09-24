/* Purpose: one body, two languages. POLY is written once over its operators;
 *   expanded with lowering.h's C operators it is the C function poly(),
 *   expanded with MeTTa's it is the equation mt_lower() installs, and the two
 *   agree on every input tried. The equation is visible to the engine as
 *   data, which a C function published with mt_def() would not be. The
 *   operators agree only where their spellings do, and % is where C and
 *   MeTTa part: WRAP lowers MeTTa's %, and C_MOD agrees with it on every
 *   combination of signs, where C's own % would not.
 * Guarantees: C and the equation agree on -20..20, the equation is stored as
 *   an atom a match can find, and C_MOD is MeTTa's % over -7..7 by -3, -2, 2
 *   and 3 [tested: make check; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"
#include "lowering.h"

#define POLY(ADD, MUL, x) ADD(MUL(3, x), 1)
#define WRAP(MOD, x, n) MOD(x, n)

static int64_t poly(int64_t x) { return POLY(C_ADD, C_MUL, x); }
static int64_t wrap(int64_t x, int64_t n) { return WRAP(C_MOD, x, n); }

int main(void)
{
    metta *m = open_engine();
    require("lower the shared body", mt_lower(m, (poly $x), POLY(M_ADD, M_MUL, $x)));
    bool agree = true;
    for (int64_t x = -20; x <= 20 && agree; x++)
        agree = mt_one_int(mt_eval(m, E("poly", x))) == poly(x);
    check("C and the equation agree on -20..20", agree);
    check_answers("the equation is an atom the space holds",
                  mt_match(m, E("=", E("poly", V("x")), V("body"))),
                  E("=", E("poly", V("x")), E("+", E("*", 3, V("x")), 1)));

    require("lower the remainder", mt_lower(m, (wrap $x $n), WRAP(M_MOD, $x, $n)));
    static const int64_t divisors[] = { -3, -2, 2, 3 };
    bool same_sign_rule = true;
    for (int64_t x = -7; x <= 7; x++)
        for (size_t i = 0; i < sizeof divisors / sizeof *divisors; i++)
            same_sign_rule = same_sign_rule && mt_one_int(mt_eval(m, E("wrap", x, divisors[i]))) == wrap(x, divisors[i]);
    check("C_MOD is MeTTa's % for every combination of signs", same_sign_rule);
    check_int("(% -7 3) is 2, where C's -7 % 3 is -1", mt_one_int(mt_eval(m, E("wrap", -7, 3))), 2);
    return done(m);
}
