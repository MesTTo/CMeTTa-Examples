/* Purpose: the accumulator fib as one body, shared by the twin that defines
 *   it and the twin that imports it, which is what an #include is: another
 *   file's definitions compiled into this one. FIB_TR expands to a C
 *   function over int64_t and to the equations install_fibsmart() lowers.
 * Assumes: the includer defines MT_SHORTHAND and includes common.h first.
 */
#ifndef FIBSMART_H
#define FIBSMART_H

#define FIB_TR(IF, EQ, ADD, SUB, SELF, n, a, b) IF(EQ(n, 0), a, SELF(SUB(n, 1), b, ADD(a, b)))
#define C_IF(c, t, e) ((c) ? (t) : (e))
#define C_EQ(a, b) ((a) == (b))
#define C_ADD(a, b) ((a) + (b))
#define C_SUB(a, b) ((a) - (b))
#define M_IF(c, t, e) (if c t e)
#define M_EQ(a, b) (== a b)
#define M_ADD(a, b) (+ a b)
#define M_SUB(a, b) (- a b)
#define M_FIB_TR(n, a, b) (fib-tr n a b)

/* int64_t holds fib(n) up to n = 92; the engine's integers are unbounded. */
static inline int64_t fib_tr(int64_t n, int64_t a, int64_t b) { return FIB_TR(C_IF, C_EQ, C_ADD, C_SUB, fib_tr, n, a, b); }
static inline int64_t fib(int64_t n) { return fib_tr(n, 0, 1); }

static inline bool install_fibsmart(metta *m)
{
    return mt_lower(m, (fib-tr $n $a $b), FIB_TR(M_IF, M_EQ, M_ADD, M_SUB, M_FIB_TR, $n, $a, $b)) &&
           mt_lower(m, (fib $n), (fib-tr $n 0 1));
}
#endif
