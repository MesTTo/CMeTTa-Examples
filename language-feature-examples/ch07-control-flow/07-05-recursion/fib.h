/* Purpose: the exponential fib as one body, shared by every twin whose
 *   original defines it, which is what an #include is: another file's
 *   definition compiled into this one. FIB expands to the recursive C
 *   function fib(), to the equation install_fib() lowers, and to the same
 *   equation as the atom fib_equation() builds, for a program that adds it
 *   itself.
 * Assumes: the includer defines MT_SHORTHAND and includes common.h first.
 */
#ifndef FIB_H
#define FIB_H
#include "lowering.h"

#define FIB(IF, LT, ADD, SUB, SELF, n) IF(LT(n, 2), n, ADD(SELF(SUB(n, 1)), SELF(SUB(n, 2))))
#define M_FIB(n) (fib n)
#define T_FIB(n) mt_expr("fib", n)

/* Time: Theta(phi^n) calls, the recursion the original's equation makes.
   int64_t holds fib(n) up to n = 92; the engine's integers are unbounded. */
static inline int64_t fib(int64_t n) { return FIB(C_IF, C_LT, C_ADD, C_SUB, fib, n); }

static inline bool install_fib(metta *m) { return mt_lower(m, (fib $N), FIB(M_IF, M_LT, M_ADD, M_SUB, M_FIB, $N)); }

/* (= (fib $N) <FIB's body>) as an atom. */
static inline mt_atom *fib_equation(void)
{
    return mt_expr("=", T_FIB(mt_var("N")), FIB(T_IF, T_LT, T_ADD, T_SUB, T_FIB, mt_var("N")));
}
#endif
