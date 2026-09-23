/* Purpose: one body, two languages. POLY is written once over its operators;
 *   expanded with C's operators it is the C function poly(), expanded with
 *   MeTTa's it is the equation mt_lower() installs, and the two agree on
 *   every input tried. The equation is visible to the engine as data, which
 *   a C function published with mt_def() would not be.
 * Guarantees: C and the equation agree on -20..20, and the equation is
 *   stored as an atom a match can find [tested: make check; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

#define POLY(ADD, MUL, x) ADD(MUL(3, x), 1)
#define C_ADD(a, b) ((a) + (b))
#define C_MUL(a, b) ((a) * (b))
#define M_ADD(a, b) (+ a b)
#define M_MUL(a, b) (* a b)

static int64_t poly(int64_t x) { return POLY(C_ADD, C_MUL, x); }

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
    return done(m);
}
