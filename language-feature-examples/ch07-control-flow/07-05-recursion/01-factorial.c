/* Purpose: recursion through a conditional, written once. FAC is a macro
 *   body over its operators and its own name: expanded with C's operators
 *   and facF it is a recursive C function, and expanded with the atom
 *   builders it is the equation mt_add installs. The engine's (facF 10) must be the C
 *   function's.
 * Guarantees: the original's claim holds [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include "lowering.h"

#define FAC(IF, EQ, MUL, SUB, SELF, n) IF(EQ(n, 0), 1, MUL(n, SELF(SUB(n, 1))))
#define T_FACF(n) E("facF", n)

static int64_t facF(int64_t n) { return FAC(C_IF, C_EQ, C_MUL, C_SUB, facF, n); }

int main(void)
{
    metta *m = open_engine();
    require("facF", mt_add(m, E("=", T_FACF(V("n")), FAC(T_IF, T_EQ, T_MUL, T_SUB, T_FACF, V("n")))));
    check_answers("(facF 10)", mt_eval(m, E("facF", 10)), facF(10));
    return done(m);
}
