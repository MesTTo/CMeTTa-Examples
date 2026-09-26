/* Purpose: lib_math, held against GMP for the exact numbers and libm for the
 *   real functions. Counts are mpz_fac_ui and mpz_bin_uiui; gcd and lcm fold
 *   mpz_gcd and mpz_lcm from their identities, 0 and 1; a ratio is an mpq_t
 *   in lowest terms, and a double's ratio is exact, since a double is a dyadic
 *   rational; rationalizing takes the first continued-fraction convergent that
 *   rounds back to the same double, SWI's own rule; roots are mpz_rootrem and
 *   modular powers mpz_powm, so neither builds a float or the full power.
 *   Rounding an exact value to a double and the square root of one come from
 *   exact_oracle.h, which 13-vector_lib shares, and a class is fpclassify's.
 *   The real functions are a C table of libm's by the library's names. C
 *   refuses what the library refuses: a non-integer where integers are
 *   folded, a zero denominator, an even root of a negative, a negative
 *   exponent or a zero modulus, an infinite ratio, an unknown function, a
 *   wrong arity and a result libm gives only as a NaN.
 * Build: cc 35-math_lib.c $(pkg-config --cflags --libs cmetta gmp) -lm
 * Assumes: GMP, found through pkg-config.
 * Guarantees: all seventy-four claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define _XOPEN_SOURCE 700 /* M_E and M_PI are XSI's */
#define MT_SHORTHAND
#if __has_include(<gmp.h>)
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include "_fixtures/exact_oracle.h"

/* A door the program needs before it can go on: on refusal, say why and stop. */
#define require(what, ok) \
    ((ok) ? (void)0 : (fprintf(stderr, "%s: %s\n", (what), mt_errmsg()), exit(EXIT_FAILURE)))

/* Whether a list holds exactly the children of want, in order, each equal
   up to renaming variables; takes both, and shows them when they differ. */
static inline bool list_is(mt_list got, mt_atom *want)
{
    bool holds = mt_ok() && want && got.len == mt_len(want);
    for (size_t i = 0; holds && i < got.len; i++)
        holds = mt_alpha_eq(got.items[i], mt_at(want, i));
    if (!holds) {
        fprintf(stderr, "  got");
        for (size_t i = 0; i < got.len; i++) fprintf(stderr, " %s", mt_show(got.items[i]));
        fprintf(stderr, "\n  want %s\n", want ? mt_show(want) : "nothing");
    }
    mt_list_free(got);
    mt_drop(want);
    return holds;
}

/* Whether a query answers exactly the children of want; takes both. */
static inline bool answers_are(mt_answers *answers, mt_atom *want)
{
    return list_is(mt_all(answers), want);
}

enum { MOST = 8 };

static mt_atom *verdict(bool holds) { return S(holds ? "fine" : "refused"); }
static mt_answers *guarded(metta *m, mt_atom *goal) { return mt_eval(m, E("if-error", E("catch", goal), "refused", "fine")); }

static bool computed(mt_atom *value)
{
    mt_drop(value);
    return value != NULL;
}

static bool integral(const mt_atom *x) { return mt_kind_of(x) == MT_INT || mt_kind_of(x) == MT_BIGINT; }

/* gcd or lcm folded over a list from its identity; NULL when an element is
   no integer. */
static mt_atom *folded(const mt_atom *list, bool lcm)
{
    mpz_t acc, x;
    mpz_init_set_ui(acc, lcm ? 1 : 0);
    mpz_init(x);
    bool ok = true;
    for (size_t i = 0; ok && i < mt_len(list); i++) {
        ok = integral(mt_at(list, i));
        if (!ok) break;
        mpq_t q;
        mpq_init(q);
        exact(q, mt_at(list, i));
        mpz_set(x, mpq_numref(q));
        mpq_clear(q);
        (lcm ? mpz_lcm : mpz_gcd)(acc, acc, x);
    }
    mt_atom *out = ok ? integer_of(acc) : NULL;
    mpz_clears(acc, x, NULL);
    return out;
}

/* A ratio as (numerator denominator), lowest terms and the sign on top. */
static mt_atom *ratio_of(const mpq_t q)
{
    mpz_t n, d;
    mpz_init_set(n, mpq_numref(q));
    mpz_init_set(d, mpq_denref(q));
    mt_atom *out = E(integer_of(n), integer_of(d));
    mpz_clears(n, d, NULL);
    return out;
}

/* A double's exact ratio; NULL for an infinity or a NaN, which have none. */
static mt_atom *exact_ratio(double x)
{
    if (!isfinite(x)) return NULL;
    mpq_t q;
    mpq_init(q);
    mpq_set_d(q, x);
    mt_atom *out = ratio_of(q);
    mpq_clear(q);
    return out;
}

/* SWI's rationalize: the continued-fraction convergents of the double's
   exact value, the first that rounds back to the same double. NULL for a
   NaN or an infinity. */
static mt_atom *rationalized(double x)
{
    if (!isfinite(x)) return NULL;
    mpq_t v, rest, convergent;
    mpz_t a, p0, q0, p1, q1, t;
    mpq_inits(v, rest, convergent, NULL);
    mpz_inits(a, t, NULL);
    mpz_init_set_ui(p0, 0), mpz_init_set_ui(q0, 1), mpz_init_set_ui(p1, 1), mpz_init_set_ui(q1, 0);
    mpq_set_d(v, x);
    mpq_set(rest, v);
    for (;;) {
        mpz_fdiv_q(a, mpq_numref(rest), mpq_denref(rest));
        mpz_mul(t, a, p1), mpz_add(t, t, p0), mpz_set(p0, p1), mpz_set(p1, t);
        mpz_mul(t, a, q1), mpz_add(t, t, q0), mpz_set(q0, q1), mpz_set(q1, t);
        mpq_set_num(convergent, p1), mpq_set_den(convergent, q1), mpq_canonicalize(convergent);
        if (rounded(convergent) == x || mpq_equal(convergent, v)) break;
        mpq_t whole;
        mpq_init(whole);
        mpq_set_z(whole, a);
        mpq_sub(rest, rest, whole);
        mpq_inv(rest, rest);
        mpq_clear(whole);
    }
    mt_atom *out = ratio_of(convergent);
    mpq_clears(v, rest, convergent, NULL);
    mpz_clears(a, p0, q0, p1, q1, t, NULL);
    return out;
}

/* The nth root truncated toward zero with its remainder; NULL for a root of
   0 or an even root of a negative. */
static mt_atom *integer_root(unsigned long n, const char *decimal)
{
    mpz_t u, root, rem;
    mpz_init_set_str(u, decimal, 10);
    mt_atom *out = NULL;
    if (n >= 1 && !(n % 2 == 0 && mpz_sgn(u) < 0)) {
        mpz_inits(root, rem, NULL);
        mpz_rootrem(root, rem, u, n);
        out = E(integer_of(root), integer_of(rem));
        mpz_clears(root, rem, NULL);
    }
    mpz_clear(u);
    return out;
}

/* base^exp mod m in [0, m); NULL for a negative exponent or a zero modulus. */
static mt_atom *power_mod(long base, long exponent, long modulus)
{
    if (exponent < 0 || modulus == 0) return NULL;
    mpz_t b, e, mod, r;
    mpz_init_set_si(b, base), mpz_init_set_si(e, exponent), mpz_init_set_si(mod, modulus), mpz_init(r);
    mpz_powm(r, b, e, mod);
    mt_atom *out = integer_of(r);
    mpz_clears(b, e, mod, r, NULL);
    return out;
}

/* The factor pairs (a n/a) with a <= n/a, in order; NULL below 1. */
static mt_atom *factor_pairs(int64_t n)
{
    if (n < 1) return NULL;
    mt_atom *out[64];
    size_t k = 0;
    for (int64_t a = 1; a * a <= n; a++)
        if (n % a == 0) out[k++] = E(a, n / a);
    return mt_exprv(k, out);
}

/* A ratio from two integers; NULL for a zero denominator. */
static mt_atom *rational(int64_t num, int64_t den)
{
    if (den == 0) return NULL;
    mpq_t q;
    mpq_init(q);
    mpq_set_si(q, num, 1);
    mpz_set_si(mpq_denref(q), den);
    mpq_canonicalize(q);
    mt_atom *out = ratio_of(q);
    mpq_clear(q);
    return out;
}

/* A square root: an exact value's through exact_oracle.h, a double's
   libm's, which keeps negative zero; NULL for a negative or an infinity. */
static mt_atom *square_root(const mt_atom *x)
{
    if (mt_kind_of(x) == MT_FLOAT) {
        double d = mt_float(x);
        return isfinite(d) && !(d < 0) ? mt_real(sqrt(d)) : NULL;
    }
    mpq_t q;
    mpq_init(q);
    exact(q, x);
    mt_atom *out = mpq_sgn(q) >= 0 ? mt_real(root(q)) : NULL;
    mpq_clear(q);
    return out;
}

/* fpclassify's classes by the library's names. */
static const char *class_of(double x)
{
    switch (fpclassify(x)) {
    case FP_NAN: return "nan";
    case FP_INFINITE: return "infinite";
    case FP_ZERO: return "zero";
    case FP_SUBNORMAL: return "subnormal";
    default: return "normal";
    }
}

/* 2^bits over or under 1 as the exact rational it is. */
static void power_of_two(mpq_t q, unsigned long bits, bool inverse)
{
    mpz_t p;
    mpz_init(p);
    mpz_ui_pow_ui(p, 2, bits);
    mpq_set_z(q, p);
    if (inverse) mpq_inv(q, q);
    mpz_clear(p);
}

/* The real functions by the library's names [source:
   lib/lib_math/lib_math.pl, math_real_function/2;
   commit=33c2d50c84b24c1a2c906225600e2a4ffdb662f1], each libm's. */
static double e_(void) { return M_E; }
static double pi_(void) { return M_PI; }
static double epsilon_(void) { return DBL_EPSILON; }
static double inf_(void) { return INFINITY; }
static double nan_(void) { return NAN; }
static double integer_part(double x) { return trunc(x); }
static double fractional_part(double x) { return x - trunc(x); }
static double toward(double x, double y) { return nexttoward(x, (long double)y); }

static const struct real {
    const char *name;
    size_t arity;
    double (*zero)(void);
    double (*one)(double);
    double (*two)(double, double);
} reals[] = {
    { "sinh", 1, NULL, sinh, NULL },     { "cosh", 1, NULL, cosh, NULL },   { "tanh", 1, NULL, tanh, NULL },
    { "asinh", 1, NULL, asinh, NULL },   { "acosh", 1, NULL, acosh, NULL }, { "atanh", 1, NULL, atanh, NULL },
    { "log10", 1, NULL, log10, NULL },   { "erf", 1, NULL, erf, NULL },     { "erfc", 1, NULL, erfc, NULL },
    { "lgamma", 1, NULL, lgamma, NULL }, { "atan2", 2, NULL, NULL, atan2 }, { "copysign", 2, NULL, NULL, copysign },
    { "nexttoward", 2, NULL, NULL, toward }, { "float_integer_part", 1, NULL, integer_part, NULL },
    { "float_fractional_part", 1, NULL, fractional_part, NULL }, { "e", 0, e_, NULL, NULL }, { "pi", 0, pi_, NULL, NULL },
    { "epsilon", 0, epsilon_, NULL, NULL }, { "inf", 0, inf_, NULL, NULL }, { "nan", 0, nan_, NULL, NULL },
};

/* A function applied to numbers; NULL for an unknown name, a wrong arity, an
   argument that is no number, or a NaN from arguments that held none. */
static mt_atom *real(const char *name, const mt_atom *args)
{
    for (size_t i = 0; i < sizeof reals / sizeof *reals; i++) {
        if (strcmp(reals[i].name, name) != 0) continue;
        double x[2];
        bool nan_in = false;
        if (mt_len(args) != reals[i].arity) return NULL;
        for (size_t k = 0; k < mt_len(args); k++) {
            mt_kind kind = mt_kind_of(mt_at(args, k));
            if (kind != MT_INT && kind != MT_FLOAT) return NULL;
            x[k] = mt_float(mt_at(args, k));
            nan_in |= isnan(x[k]);
        }
        double y = reals[i].arity == 0 ? reals[i].zero() : reals[i].arity == 1 ? reals[i].one(x[0]) : reals[i].two(x[0], x[1]);
        return isnan(y) && !nan_in && reals[i].zero == NULL ? NULL : mt_real(y);
    }
    return NULL;
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_math", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_math")))));
    mpz_t z;
    mpz_init(z);

    /* Counts, gcd and lcm. */
    mpz_fac_ui(z, 20);
    assert(answers_are(mt_eval(m, E("factorial", 20)), E(integer_of(z))) && "factorial");
    mpz_bin_uiui(z, 52, 5);
    assert(answers_are(mt_eval(m, E("binomial", 52, 5)), E(integer_of(z))) && "binomial");
    mt_atom *lists[] = { mt_unit(), E(0, 0), E(-18, 24, 30), E(mt_bigint("18446744073709551616"), mt_bigint("36893488147419103232")),
                         mt_unit(), E(-6, 8, 15), E(0, 5) };
    const char *fold_claims[] = { "the gcd of none", "of zeros", "of signed integers", "of wide integers",
                                  "the lcm of none", "of signed integers", "with a zero" };
    for (size_t i = 0; i < 7; i++) {
        bool lcm = i >= 4;
        assert(answers_are(mt_eval(m, E(lcm ? "math-lcm" : "math-gcd", mt_keep(lists[i]))), E(folded(lists[i], lcm))) && fold_claims[i]);
        mt_drop(lists[i]);
    }

    /* Rationals, exact. */
    mpq_t q, r;
    mpq_inits(q, r, NULL);
    mpq_set_si(q, 1, 3);
    mpq_set_si(r, 3, 1);
    mpq_mul(r, q, r);
    assert(answers_are(mt_eval(m, E("*", E("math-rational", 1, 3), 3)), E(integer_of(mpq_numref(r)))) && "a third times three");
    assert(answers_are(mt_eval(m, E("math-ratio", E("math-rational", 10, -20))), E(rational(10, -20))) && "lowest terms, sign on top");
    mpq_set_si(q, 6, 3);
    mpq_canonicalize(q);
    assert(answers_are(mt_eval(m, E("math-class", E("math-rational", 6, 3))), E(S(mpz_cmp_ui(mpq_denref(q), 1) == 0 ? "integer" : "rational")))
           && "a whole ratio is an integer");
    mpq_set_si(q, 1, 3);
    assert(answers_are(mt_eval(m, E("math-class", E("math-rational", 1, 3))), E(S(mpz_cmp_ui(mpq_denref(q), 1) == 0 ? "integer" : "rational")))
           && "a third is rational");
    assert(answers_are(mt_eval(m, E("math-ratio", 0.1)), E(exact_ratio(0.1))) && "a double's exact ratio");
    assert(answers_are(mt_eval(m, E("math-ratio", E("math-rational", 0.1))), E(exact_ratio(0.1))) && "math-rational keeps it");
    assert(answers_are(mt_eval(m, E("math-ratio", -0.0)), E(exact_ratio(-0.0))) && "negative zero is zero");
    assert(answers_are(mt_eval(m, E("math-ratio", E("math-rationalize", 0.1))), E(rationalized(0.1))) && "rationalizing");
    assert(answers_are(mt_eval(m, E("math-rationalize", 42)), E((int64_t)42)) && "an integer rationalizes to itself");
    mpq_set_si(q, 2, 3);
    assert(answers_are(mt_eval(m, E("math-ratio", E("math-rationalize", E("math-rational", 2, 3)))), E(ratio_of(q))) && "so does a ratio");

    /* Roots and modular powers. */
    assert(answers_are(mt_eval(m, E("math-integer-root", 2, 101)), E(integer_root(2, "101"))) && "a square root and remainder");
    assert(answers_are(mt_eval(m, E("math-integer-root", 3, -28)), E(integer_root(3, "-28"))) && "an odd root of a negative");
    assert(answers_are(mt_eval(m, E("math-integer-root", 1, -42)), E(integer_root(1, "-42"))) && "the first root");
    assert(answers_are(mt_eval(m, E("math-integer-root", 2, mt_bigint("340282366920938463463374607431768211456"))), E(integer_root(2, "340282366920938463463374607431768211456")))
           && "a wide root");
    assert(answers_are(mt_eval(m, E("math-power-mod", 2, 100, 1000)), E(power_mod(2, 100, 1000))) && "a modular power");
    assert(answers_are(mt_eval(m, E("math-power-mod", -2, 3, 5)), E(power_mod(-2, 3, 5))) && "of a negative base");
    assert(answers_are(mt_eval(m, E("math-power-mod", 42, 0, 1)), E(power_mod(42, 0, 1))) && "modulo one");
    assert(answers_are(mt_eval(m, E("collapse", E("math-factor-pairs", 1))), E(factor_pairs(1))) && "the factor pairs of one");
    mt_atom *pairs36 = factor_pairs(36);
    assert(answers_are(mt_eval(m, E("collapse", E("math-factor-pairs", 36))), E(mt_keep(pairs36))) && "of 36");
    assert(answers_are(mt_eval(m, E("once", E("math-factor-pairs", 36))), E(mt_keep(mt_at(pairs36, 0)))) && "the first");
    mt_drop(pairs36);

    /* Square roots of exact values, rounded once. */
    mt_atom *four = mt_num(4);
    assert(answers_are(mt_eval(m, E("math-sqrt", mt_keep(four))), E(square_root(four))) && "the root of 4");
    mt_drop(four);
    power_of_two(q, 2000, false);
    power_of_two(r, 1000, false);
    require("the root of 2^2000 rounds to 2^1000", root(q) == rounded(r));
    assert(answers_are(mt_eval(m, E("==", E("math-sqrt", E("bit-shift-left", 1, 2000)), E("math-float", E("bit-shift-left", 1, 1000)))), E(B(root(q) == rounded(r))))
           && "a wide root");
    power_of_two(q, 2000, true);
    power_of_two(r, 1000, true);
    assert(answers_are(mt_eval(m, E("==", E("math-sqrt", E("math-rational", 1, E("bit-shift-left", 1, 2000))),
                                    E("math-float", E("math-rational", 1, E("bit-shift-left", 1, 1000))))), E(B(root(q) == rounded(r))))
           && "a narrow one");
    assert(answers_are(mt_eval(m, E("math-real", "copysign", E(1.0, E("math-sqrt", -0.0)))), E(copysign(1.0, sqrt(-0.0)))) && "negative zero keeps its sign");
    mt_atom *minus_one = mt_num(-1), *infinity = mt_real(INFINITY);
    assert(answers_are(guarded(m, E("math-sqrt", mt_keep(minus_one))), E(verdict(computed(square_root(minus_one))))) && "no root of a negative");
    assert(answers_are(guarded(m, E("math-sqrt", E("math-real", "inf", mt_unit()))), E(verdict(computed(square_root(infinity))))) && "nor of infinity");
    mt_drop(minus_one);
    mt_drop(infinity);

    /* Rounding exact values to doubles. */
    mpq_set_si(q, 1, 10);
    assert(answers_are(mt_eval(m, E("math-float", E("math-rational", 1, 10))), E(rounded(q))) && "a tenth");
    assert(answers_are(mt_eval(m, E("math-class", E("math-float", 1))), E(S(class_of(1.0)))) && "one is normal");
    assert(answers_are(mt_eval(m, E("math-class", E("math-float", -0.0))), E(S(class_of(-0.0)))) && "negative zero is zero");
    assert(answers_are(mt_eval(m, E("math-real", "copysign", E(1.0, E("math-float", -0.0)))), E(copysign(1.0, -0.0))) && "and keeps its sign");
    mpz_ui_pow_ui(z, 2, 1129);
    mpq_set_ui(q, 1, 1);
    mpz_set_str(mpq_numref(q), "18014398509481985", 10);
    mpz_set(mpq_denref(q), z);
    mpq_canonicalize(q);
    assert(answers_are(mt_eval(m, E("math-class", E("math-float", E("math-rational", mt_bigint("18014398509481985"), E("bit-shift-left", 1, 1129))))), E(S(class_of(rounded(q)))))
           && "a subnormal");
    power_of_two(q, 2000, false);
    assert(answers_are(mt_eval(m, E("math-class", E("math-float", E("bit-shift-left", 1, 2000)))), E(S(class_of(rounded(q))))) && "an overflow");

    /* The real functions: libm's, by name. */
    assert(answers_are(mt_eval(m, E("size-atom", E("math-real-functions"))), E((int64_t)(sizeof reals / sizeof *reals))) && "twenty functions");
    static const struct {
        const char *name;
        int64_t arg;
    } at_int[] = { { "sinh", 0 }, { "cosh", 0 }, { "tanh", 0 }, { "asinh", 0 }, { "acosh", 1 }, { "atanh", 0 },
                   { "log10", 100 }, { "erf", 0 }, { "erfc", 0 }, { "lgamma", 1 } };
    for (size_t i = 0; i < sizeof at_int / sizeof *at_int; i++) {
        mt_atom *args = E(at_int[i].arg);
        assert(answers_are(mt_eval(m, E("math-real", at_int[i].name, mt_keep(args))), E(real(at_int[i].name, args))) && at_int[i].name);
        mt_drop(args);
    }
    mt_atom *ones = E(1, 1), *step = E(1.0, 2.0), *minus = E(-1.75), *none = mt_unit();
    mt_atom *quarter = real("atan2", ones), *pi = real("pi", none);
    assert(answers_are(mt_eval(m, E("math-real", "atan2", mt_keep(ones))), E(mt_real(mt_float(pi) / 4))) && "atan2");
    require("libm's atan2 is pi over 4", mt_float(quarter) == mt_float(pi) / 4);
    assert(answers_are(mt_eval(m, E("math-real", "nexttoward", mt_keep(step))), E(real("nexttoward", step))) && "nexttoward");
    assert(answers_are(mt_eval(m, E("math-real", "float_integer_part", mt_keep(minus))), E(real("float_integer_part", minus))) && "the integer part");
    assert(answers_are(mt_eval(m, E("math-real", "float_fractional_part", mt_keep(minus))), E(real("float_fractional_part", minus))) && "the fractional part");
    mt_atom *e = real("e", none);
    assert(answers_are(mt_eval(m, E("<", 2.7, E("math-real", "e", mt_unit()))), E(B(2.7 < mt_float(e)))) && "e");
    assert(answers_are(mt_eval(m, E("math-real", "epsilon", mt_unit())), E(real("epsilon", none))) && "epsilon");
    assert(answers_are(mt_eval(m, E("math-class", E("math-real", "inf", mt_unit()))), E(S(class_of(INFINITY)))) && "infinity");
    assert(answers_are(mt_eval(m, E("math-class", E("math-real", "nan", mt_unit()))), E(S(class_of(NAN)))) && "not a number");

    /* Refusals. */
    mt_atom *half = E(1, 2.5), *zero_half = E(0, 2.5), *one = E(1), *x = E(T("x")), *zero = E(0);
    assert(answers_are(guarded(m, E("math-gcd", mt_keep(half))), E(verdict(computed(folded(half, false))))) && "a gcd of a float");
    assert(answers_are(guarded(m, E("math-lcm", mt_keep(zero_half))), E(verdict(computed(folded(zero_half, true))))) && "an lcm of one");
    assert(answers_are(guarded(m, E("math-rational", 1, 0)), E(verdict(computed(rational(1, 0))))) && "a zero denominator");
    assert(answers_are(guarded(m, E("math-ratio", E("math-real", "inf", mt_unit()))), E(verdict(computed(exact_ratio(INFINITY))))) && "infinity has no ratio");
    assert(answers_are(guarded(m, E("math-rationalize", E("math-real", "nan", mt_unit()))), E(verdict(computed(rationalized(NAN))))) && "a NaN has no rationalization");
    assert(answers_are(guarded(m, E("math-integer-root", 0, 5)), E(verdict(computed(integer_root(0, "5"))))) && "no zeroth root");
    assert(answers_are(guarded(m, E("math-integer-root", 2, -1)), E(verdict(computed(integer_root(2, "-1"))))) && "no even root of a negative");
    assert(answers_are(guarded(m, E("math-power-mod", 2, -1, 5)), E(verdict(computed(power_mod(2, -1, 5))))) && "no negative exponent");
    assert(answers_are(guarded(m, E("math-power-mod", 2, 3, 0)), E(verdict(computed(power_mod(2, 3, 0))))) && "no zero modulus");
    assert(answers_are(guarded(m, E("math-factor-pairs", 0)), E(verdict(computed(factor_pairs(0))))) && "no factor pairs of zero");
    assert(answers_are(guarded(m, E("math-real", "missing", mt_keep(one))), E(verdict(computed(real("missing", one))))) && "no such function");
    assert(answers_are(guarded(m, E("math-real", "atan2", mt_keep(one))), E(verdict(computed(real("atan2", one))))) && "atan2 takes two");
    assert(answers_are(guarded(m, E("math-real", "erf", mt_keep(x))), E(verdict(computed(real("erf", x))))) && "numbers only");
    assert(answers_are(guarded(m, E("math-real", "acosh", mt_keep(zero))), E(verdict(computed(real("acosh", zero))))) && "acosh below one");

    mt_atom *held[] = { ones, step, minus, none, quarter, pi, e, half, zero_half, one, x, zero };
    for (size_t i = 0; i < sizeof held / sizeof *held; i++) mt_drop(held[i]);
    mpq_clears(q, r, NULL);
    mpz_clear(z);
    mt_close(m);
    return 0;
}
#else
#include <stdio.h>

/* Without GMP's headers the program only says what it needs. */
int main(void)
{
    fputs("35-math_lib.c needs GMP: install its development files, then build with\n"
          "cc 35-math_lib.c $(pkg-config --cflags --libs cmetta gmp) -lm\n", stderr);
    return 77;
}
#endif
