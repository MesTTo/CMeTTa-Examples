/* Purpose: the exponential fib as one body, shared by every twin whose
 *   original defines it, which is what an #include is: another file's
 *   definition compiled into this one. FIB expands to the recursive C
 *   function fib() and to the equation fib_equation() builds as an atom,
 *   which install_fib() adds to a space and a program can add itself.
 * Assumes: the includer defines MT_SHORTHAND and includes common.h first.
 */
#ifndef FIB_H
#define FIB_H
#include "lowering.h"

#define FIB(IF, LT, ADD, SUB, SELF, n) IF(LT(n, 2), n, ADD(SELF(SUB(n, 1)), SELF(SUB(n, 2))))
#define T_FIB(n) mt_expr("fib", n)

/* Time: Theta(phi^n) calls, the recursion the original's equation makes.
   int64_t holds fib(n) up to n = 92; the engine's integers are unbounded. */
static inline int64_t fib(int64_t n) { return FIB(C_IF, C_LT, C_ADD, C_SUB, fib, n); }

/* (= (fib $N) <FIB's body>) as an atom. */
static inline mt_atom *fib_equation(void)
{
    return mt_expr("=", T_FIB(mt_var("N")), FIB(T_IF, T_LT, T_ADD, T_SUB, T_FIB, mt_var("N")));
}

static inline bool install_fib(metta *m) { return mt_add(m, fib_equation()); }
#endif
