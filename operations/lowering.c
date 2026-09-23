/* Purpose: Use one arithmetic body from C and from a lowered equation.
 * Owns resources: local C handles are released before exit; a failed check
 *   terminates the example process.
 * Guarantees: the assertions below hold [tested: make check; commit=WORKTREE].
 * Open Obligations: None.
 */
#include "common.h"
#define POLY(ADD, MUL, x) ADD(MUL(3, x), 1)
#define C_ADD(a,b) ((a)+(b))
#define C_MUL(a,b) ((a)*(b))
#define M_ADD(a,b) (+ a b)
#define M_MUL(a,b) (* a b)
static int64_t poly(int64_t x) { return POLY(C_ADD, C_MUL, x); }
int main(void)
{
    metta *m = open_engine();
    check("lower shared body", mt_lower(m, (poly $x), POLY(M_ADD, M_MUL, $x)));
    for (int64_t x = -20; x <= 20; ++x)
        check("C and equation agree", mt_one_int(mt_eval(m, mt_expr("poly", x))) == poly(x));
    return done(m, "lowering");
}

