/* Purpose: recursion through a conditional, written once. FAC is a macro
 *   body over its operators and its own name: expanded with C's operators
 *   and facF it is a recursive C function, and expanded to MeTTa tokens it
 *   is the equation mt_lower installs. The engine's (facF 10) must be the C
 *   function's.
 * Guarantees: the original's claim holds [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"
#include "lowering.h"

#define FAC(IF, EQ, MUL, SUB, SELF, n) IF(EQ(n, 0), 1, MUL(n, SELF(SUB(n, 1))))
#define M_FACF(n) (facF n)

static int64_t facF(int64_t n) { return FAC(C_IF, C_EQ, C_MUL, C_SUB, facF, n); }

int main(void)
{
    metta *m = open_engine();
    require("facF", mt_lower(m, (facF $n), FAC(M_IF, M_EQ, M_MUL, M_SUB, M_FACF, $n)));
    check_answers("(facF 10)", mt_eval(m, E("facF", 10)), facF(10));
    return done(m);
}
