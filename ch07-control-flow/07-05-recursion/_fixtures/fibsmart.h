/* Purpose: the accumulator fib as one body, shared by the twin that defines
 *   it and the twin that imports it, which is what an #include is: another
 *   file's definitions compiled into this one. FIB_TR expands to a C
 *   function over int64_t and to the equation atoms install_fibsmart() adds.
 * Assumes: the includer defines MT_SHORTHAND before its first include.
 */
#ifndef FIBSMART_H
#define FIBSMART_H
#include <cmetta.h>

/* Each MeTTa operator twice, for a body written once over its operators:
   C_X computes in C, and T_X builds the atom (X a b). */
#define C_IF(c, t, e) ((c) ? (t) : (e))
#define T_IF(c, t, e) mt_expr("if", c, t, e)
#define C_EQ(a, b) ((a) == (b))
#define T_EQ(a, b) mt_expr("==", a, b)
#define C_ADD(a, b) ((a) + (b))
#define T_ADD(a, b) mt_expr("+", a, b)
#define C_SUB(a, b) ((a) - (b))
#define T_SUB(a, b) mt_expr("-", a, b)

#define FIB_TR(IF, EQ, ADD, SUB, SELF, n, a, b) IF(EQ(n, 0), a, SELF(SUB(n, 1), b, ADD(a, b)))
#define T_FIB_TR(n, a, b) mt_expr("fib-tr", n, a, b)

/* int64_t holds fib(n) up to n = 92; the engine's integers are unbounded. */
static inline int64_t fib_tr(int64_t n, int64_t a, int64_t b) { return FIB_TR(C_IF, C_EQ, C_ADD, C_SUB, fib_tr, n, a, b); }
static inline int64_t fib(int64_t n) { return fib_tr(n, 0, 1); }

static inline bool install_fibsmart(metta *m)
{
    return mt_add(m, mt_expr("=", T_FIB_TR(mt_var("n"), mt_var("a"), mt_var("b")),
                             FIB_TR(T_IF, T_EQ, T_ADD, T_SUB, T_FIB_TR, mt_var("n"), mt_var("a"), mt_var("b")))) &&
           mt_add(m, mt_expr("=", mt_expr("fib", mt_var("n")), T_FIB_TR(mt_var("n"), 0, 1)));
}
#endif
