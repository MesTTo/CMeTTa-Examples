/* Purpose: the exponential fib, written once and run in both languages.
 *   FIB expands to a recursive C function and to the equation mt_lower
 *   installs; the engine runs it under a raised branch budget, a pragma
 *   scoped to the one evaluation, and must answer what C computed.
 * Guarantees: the original's claim holds [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"
#include "lowering.h"

#define FIB(IF, LT, ADD, SUB, SELF, n) IF(LT(n, 2), n, ADD(SELF(SUB(n, 1)), SELF(SUB(n, 2))))
#define M_FIB(n) (fib n)

static int64_t fib(int64_t n) { return FIB(C_IF, C_LT, C_ADD, C_SUB, fib, n); }

int main(void)
{
    metta *m = open_engine();
    require("fib", mt_lower(m, (fib $N), FIB(M_IF, M_LT, M_ADD, M_SUB, M_FIB, $N)));
    check_answers("(fib 30) under a raised budget",
                  mt_eval(m, E("with-pragma!", E(E("max-stack-depth", 100000000)), E("fib", 30))), fib(30));
    return done(m);
}
