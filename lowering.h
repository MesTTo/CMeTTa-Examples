/* Purpose: each MeTTa operator's spellings, for a body written once and
 *   expanded more than once, which cmetta.h calls ONE BODY, BOTH LANGUAGES.
 *   C_X is the C expression, M_X the MeTTa tokens mt_lower() stringifies,
 *   and T_X the atom mt_expr() builds, which is how a program names the
 *   equation it lowered when it removes it. A body takes its operators as
 *   parameters and names none of these itself:
 *
 *       #define TWICE(ADD, x) ADD(x, x)
 *       static int64_t twice(int64_t x) { return TWICE(C_ADD, x); }
 *       require("twice", mt_lower(m, (twice $x), TWICE(M_ADD, $x)));
 *
 *   The C function is the oracle and the lowered equation is what the engine
 *   runs, so a twin checking one against the other checks the program twice.
 * Assumes: the includer includes common.h first.
 * Guarantees:
 *   - each C_X computes what its M_X computes, over the operands the C types
 *     can hold: C's integers are int64_t where the engine's are unbounded, so
 *     C_ADD, C_SUB and C_MUL agree only while the result fits
 *     [tested: make twins; commit=4fe77404069bc1a630ecc9e7860856a1117a200c]
 *   - C_MOD is MeTTa's %, which is Prolog's mod and takes the divisor's sign,
 *     where C's % takes the dividend's: (% -7 3) is 2 and -7 % 3 is -1
 *     [source: engine/metta/operators.pl:154, R is A mod B;
 *     commit=d4a365c16bdf1801f9839597e56ecfcc8c2b7a0c]
 *   - C_DIV is MeTTa's / on floats, a zero divisor included, which answers
 *     the signed infinity in both; on two integers MeTTa answers an integer
 *     when the quotient is exact and a float when it is not, where C
 *     truncates, so a body divides doubles [source: engine/metta/operators.pl,
 *     '/'/3 and metta_saturating_recover/4;
 *     commit=8d651070dedaa190e25cc388c029172a63e967be] [tested: make check;
 *     commit=4fe77404069bc1a630ecc9e7860856a1117a200c]
 *   - C_MIN and C_MAX are SWI's min and max on operands that are neither NaN
 *     nor zeros of opposite sign, where SWI answers NaN and prefers -0.0 for
 *     min [source: swipl-devel V10.1.14 src/pl-arith.c, ar_min and ar_max]
 *     [tested: make check; commit=4fe77404069bc1a630ecc9e7860856a1117a200c]
 */
#ifndef EXAMPLES_LOWERING_H
#define EXAMPLES_LOWERING_H

#define C_IF(c, t, e) ((c) ? (t) : (e))
#define M_IF(c, t, e) (if c t e)
#define T_IF(c, t, e) mt_expr("if", c, t, e)

#define C_EQ(a, b) ((a) == (b))
#define M_EQ(a, b) (== a b)
#define T_EQ(a, b) mt_expr("==", a, b)
#define C_LT(a, b) ((a) < (b))
#define M_LT(a, b) (< a b)
#define T_LT(a, b) mt_expr("<", a, b)
#define C_GT(a, b) ((a) > (b))
#define M_GT(a, b) (> a b)
#define T_GT(a, b) mt_expr(">", a, b)
#define C_LE(a, b) ((a) <= (b))
#define M_LE(a, b) (<= a b)
#define T_LE(a, b) mt_expr("<=", a, b)

#define C_ADD(a, b) ((a) + (b))
#define M_ADD(a, b) (+ a b)
#define T_ADD(a, b) mt_expr("+", a, b)
#define C_SUB(a, b) ((a) - (b))
#define M_SUB(a, b) (- a b)
#define T_SUB(a, b) mt_expr("-", a, b)
#define C_MUL(a, b) ((a) * (b))
#define M_MUL(a, b) (* a b)
#define T_MUL(a, b) mt_expr("*", a, b)
#define C_DIV(a, b) ((a) / (b))
#define M_DIV(a, b) (/ a b)
#define T_DIV(a, b) mt_expr("/", a, b)
#define C_MOD(a, b) floor_mod(a, b)
#define M_MOD(a, b) (% a b)
#define T_MOD(a, b) mt_expr("%", a, b)

#define C_MIN(a, b) ((a) < (b) ? (a) : (b))
#define M_MIN(a, b) (min a b)
#define T_MIN(a, b) mt_expr("min", a, b)
#define C_MAX(a, b) ((a) > (b) ? (a) : (b))
#define M_MAX(a, b) (max a b)
#define T_MAX(a, b) mt_expr("max", a, b)

#define C_AND(a, b) ((a) && (b))
#define M_AND(a, b) (and a b)
#define T_AND(a, b) mt_expr("and", a, b)

/* On booleans, xor is inequality. */
#define C_XOR(a, b) ((a) != (b))
#define M_XOR(a, b) (xor a b)
#define T_XOR(a, b) mt_expr("xor", a, b)

/* The remainder whose sign is the divisor's, which is what Prolog's mod and
   so MeTTa's % answer. Undefined for b == 0, as MeTTa's is an error there. */
static inline int64_t floor_mod(int64_t a, int64_t b)
{
    int64_t r = a % b;
    return r != 0 && (r < 0) != (b < 0) ? r + b : r;
}
#endif
