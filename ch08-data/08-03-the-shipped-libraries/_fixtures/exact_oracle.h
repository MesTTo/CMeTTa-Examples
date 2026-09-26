/* Purpose: GMP as the oracle for the twins that read numbers exactly and
 *   round them to doubles, shared by 13-vector_lib, 35-math_lib,
 *   37-statistics_lib and 42-database_lib: which atoms are numbers, an
 *   exact rational rounded once to the nearest double at the final binary64
 *   quantum, ties to even, so a subnormal rounds once too, and the square
 *   root of an exact rational through a 109-bit integer root rounded to odd
 *   first, which keeps rounding once. Static inline, so a twin that uses
 *   part of it compiles clean.
 * Assumes: GMP, and the includer defines MT_SHORTHAND before its first include.
 */
#ifndef EXACT_ORACLE_H
#define EXACT_ORACLE_H
#include <cmetta.h>
#include <string.h>
#include <gmp.h>
#include <math.h>

static inline long top_bit(const mpz_t z) { return (long)mpz_sizeinbase(z, 2) - 1; }

/* a/b as n/d with its binary point moved: n << s over d, or n over d << -s. */
static inline void shifted(mpz_t a, mpz_t b, const mpz_t n, const mpz_t d, long s)
{
    if (s >= 0) { mpz_mul_2exp(a, n, (mp_bitcnt_t)s); mpz_set(b, d); }
    else { mpz_set(a, n); mpz_mul_2exp(b, d, (mp_bitcnt_t)-s); }
}

/* n/d for positive n and d, to the nearest double, ties to even, rounded at
   the final binary64 quantum so a subnormal rounds once too
   [source: https://github.com/python/cpython/blob/ebf955df7a89ed0c7968f79faec1de49f61ed7cb/Objects/longobject.c#L4508,
   the method lib/lib_vector/lib_vector.pl positive_float/3 follows;
   commit=33c2d50c84b24c1a2c906225600e2a4ffdb662f1]. */
static inline double positive_float(const mpz_t n, const mpz_t d)
{
    mpz_t a, b, q, r;
    double out;
    long difference = top_bit(n) - top_bit(d), exponent, shift;
    mpz_inits(a, b, q, r, NULL);
    shifted(a, b, n, d, -difference);
    exponent = mpz_cmp(a, b) < 0 ? difference - 1 : difference;
    if (exponent > 1023) out = HUGE_VAL;
    else if (exponent < -1075) out = 0.0;
    else {
        shift = exponent - 52 > -1074 ? exponent - 52 : -1074;
        shifted(a, b, n, d, -shift);
        mpz_fdiv_qr(q, r, a, b);
        mpz_mul_2exp(r, r, 1);
        int half = mpz_cmp(r, b);
        if (half > 0 || (half == 0 && mpz_odd_p(q))) mpz_add_ui(q, q, 1);
        out = mpz_sgn(q) == 0 ? 0.0 : top_bit(q) + shift > 1023 ? HUGE_VAL : ldexp(mpz_get_d(q), (int)shift);
    }
    mpz_clears(a, b, q, r, NULL);
    return out;
}

static inline double rounded(const mpq_t v)
{
    if (mpq_sgn(v) == 0) return 0.0;
    mpz_t magnitude;
    mpz_init(magnitude);
    mpz_abs(magnitude, mpq_numref(v));
    double out = positive_float(magnitude, mpq_denref(v));
    mpz_clear(magnitude);
    return mpq_sgn(v) < 0 ? -out : out;
}

static inline long floor_half(long x) { return x >= 0 ? x / 2 : -((1 - x) / 2); }

/* The square root of a nonnegative rational, rounded once: a 109-bit integer
   root, made odd when it is not exact, then rounded as any quotient is
   [source: https://github.com/python/cpython/blob/ebf955df7a89ed0c7968f79faec1de49f61ed7cb/Lib/statistics.py#L1695-L1721,
   which lib/lib_vector/lib_vector.pl fraction_sqrt/2 translates;
   commit=33c2d50c84b24c1a2c906225600e2a4ffdb662f1]. */
static inline double root(const mpq_t v)
{
    if (mpq_sgn(v) == 0) return 0.0;
    mpz_t a, b, whole, r, check, one, scaled_n, scaled_d;
    mpz_inits(a, b, whole, r, check, scaled_n, scaled_d, NULL);
    mpz_init_set_ui(one, 1);
    long shift = floor_half(top_bit(mpq_numref(v)) - top_bit(mpq_denref(v)) - 109);
    shifted(a, b, mpq_numref(v), mpq_denref(v), -2 * shift);
    mpz_fdiv_q(whole, a, b);
    mpz_sqrt(r, whole);
    mpz_mul(check, r, r);
    mpz_mul(check, check, b);
    if (mpz_cmp(check, a) != 0) mpz_setbit(r, 0);
    shifted(scaled_n, scaled_d, r, one, shift);
    double out = positive_float(scaled_n, scaled_d);
    mpz_clears(a, b, whole, r, check, one, scaled_n, scaled_d, NULL);
    return out;
}

/* A MeTTa Number: the five kinds C splits the wire's one number tag into. */
static inline bool is_number(const mt_atom *x)
{
    mt_kind k = mt_kind_of(x);
    return k == MT_INT || k == MT_BIGINT || k == MT_RATIONAL || k == MT_BIGRATIONAL || k == MT_FLOAT;
}

/* A number atom as the exact rational it is: an integer or a ratio of any
   width, whose wide kinds carry the canonical text mpq_set_str() reads, or a
   double, which is a dyadic rational. */
static inline void exact(mpq_t q, const mt_atom *x)
{
    switch (mt_kind_of(x)) {
    case MT_INT: mpq_set_si(q, (long)mt_int(x), 1); break;
    case MT_BIGINT:
    case MT_BIGRATIONAL: mpq_set_str(q, mt_name(x), 10); break;
    case MT_RATIONAL: { mt_ratio r = mt_ratio_of(x); mpq_set_si(q, (long)r.num, (unsigned long)r.den); mpq_canonicalize(q); break; }
    default: mpq_set_d(q, mt_float(x)); break;
    }
}

/* A number atom as the nearest double, ties to even: a float is itself, and
   an exact number rounds once, to an infinity past the largest double. */
static inline double nearest(const mt_atom *x)
{
    if (mt_kind_of(x) == MT_FLOAT) return mt_float(x);
    mpq_t q;
    mpq_init(q);
    exact(q, x);
    double out = rounded(q);
    mpq_clear(q);
    return out;
}

/* GMP's own text back to GMP's allocator. */
static inline void gmp_release(char *text)
{
    void (*release)(void *, size_t);
    mp_get_memory_functions(NULL, NULL, &release);
    release(text, strlen(text) + 1);
}

/* An exact integer atom from GMP: an Int when it fits, a BigInt when not. */
static inline mt_atom *integer_of(const mpz_t z)
{
    char *digits = mpz_get_str(NULL, 10, z);
    mt_atom *out = mt_bigint(digits);
    gmp_release(digits);
    return out;
}

/* An exact rational atom from a canonical mpq: GMP writes N/D, or N alone
   for a whole one, and the constructor for that text picks the kind the value
   is, Int, BigInt, Rational or BigRational. */
static inline mt_atom *rational_of(const mpq_t q)
{
    char *text = mpq_get_str(NULL, 10, q);
    mt_atom *out = strchr(text, '/') ? mt_bigrational(text) : mt_bigint(text);
    gmp_release(text);
    return out;
}

#endif
