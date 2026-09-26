/* Purpose: lib_vector, held against GMP, the arithmetic SWI's own rationals
 *   run on. Each vector C passes the engine is read into exact mpq_t values:
 *   an integer, a ratio or a double are all exact rationals, so a dot product
 *   is an exact sum rounded once, a norm or a distance the exact square root
 *   of an exact sum rounded once, and a cosine or a normalized coordinate the
 *   exact square root of an exact ratio with its sign put back. Rounding is to
 *   the nearest double at the final binary64 quantum, ties to even, and a
 *   root goes through a 109-bit integer square root rounded to odd first,
 *   which is how rounding once stays rounding once. Exact integer arithmetic
 *   stays in C integers and a quotient reduces by its gcd. A random vector is
 *   checked for what C can know of it: its size, its unit length. Each
 *   refusal is C's precondition failing on the atoms it passed.
 * Build: cc 13-vector_lib.c $(pkg-config --cflags --libs cmetta gmp)
 * Assumes: GMP, found through pkg-config; the rounding and the root come
 *   from exact_oracle.h, which 35-math_lib shares.
 * Guarantees: all forty-three claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#if __has_include(<gmp.h>)
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
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

#define COUNT(array) (sizeof (array) / sizeof *(array))
enum { MOST = 8 };

static bool finite_all(const mt_atom *v)
{
    for (size_t i = 0; i < mt_len(v); i++) {
        const mt_atom *x = mt_at(v, i);
        if (mt_kind_of(x) == MT_FLOAT && !isfinite(mt_float(x))) return false;
    }
    return true;
}

/* sum of x_i * y_i, exactly. */
static void inner(mpq_t sum, const mt_atom *x, const mt_atom *y)
{
    mpq_t a, b;
    mpq_inits(a, b, NULL);
    mpq_set_ui(sum, 0, 1);
    for (size_t i = 0; i < mt_len(x); i++) {
        exact(a, mt_at(x, i));
        exact(b, mt_at(y, i));
        mpq_mul(a, a, b);
        mpq_add(sum, sum, a);
    }
    mpq_clears(a, b, NULL);
}

static double dot(const mt_atom *x, const mt_atom *y)
{
    mpq_t s;
    mpq_init(s);
    inner(s, x, y);
    double out = rounded(s);
    mpq_clear(s);
    return out;
}

/* The IEEE rule for a length with a nonfinite component: NaN if any is NaN,
   otherwise infinity. */
static double nonfinite_length(const mt_atom *v)
{
    for (size_t i = 0; i < mt_len(v); i++)
        if (mt_kind_of(mt_at(v, i)) == MT_FLOAT && isnan(mt_float(mt_at(v, i)))) return NAN;
    return HUGE_VAL;
}

static double norm(const mt_atom *v)
{
    if (!finite_all(v)) return nonfinite_length(v);
    mpq_t s;
    mpq_init(s);
    inner(s, v, v);
    double out = root(s);
    mpq_clear(s);
    return out;
}

/* cos = dot / sqrt(|x|^2 |y|^2) = sign(dot) sqrt(dot^2 / (|x|^2 |y|^2)),
   one exact ratio under one rounded root; a zero vector has no direction. */
static double cosine(const mt_atom *x, const mt_atom *y)
{
    mpq_t d, xx, yy;
    mpq_inits(d, xx, yy, NULL);
    inner(d, x, y);
    inner(xx, x, x);
    inner(yy, y, y);
    double out = NAN;
    if (mpq_sgn(xx) > 0 && mpq_sgn(yy) > 0) {
        int sign = mpq_sgn(d);
        mpq_mul(d, d, d);
        mpq_mul(xx, xx, yy);
        mpq_div(d, d, xx);
        out = copysign(root(d), sign < 0 ? -1.0 : 1.0);
    }
    mpq_clears(d, xx, yy, NULL);
    return out;
}

static mt_atom *normalized(const mt_atom *v)
{
    mt_atom *kids[MOST];
    mpq_t s, c;
    mpq_inits(s, c, NULL);
    inner(s, v, v);
    for (size_t i = 0; i < mt_len(v); i++) {
        exact(c, mt_at(v, i));
        int sign = mpq_sgn(c);
        mpq_mul(c, c, c);
        mpq_div(c, c, s);
        kids[i] = mt_real(copysign(root(c), sign < 0 ? -1.0 : 1.0));
    }
    mpq_clears(s, c, NULL);
    return mt_exprv(mt_len(v), kids);
}

static double distance(const mt_atom *x, const mt_atom *y)
{
    mpq_t s, a, b;
    mpq_inits(s, a, b, NULL);
    for (size_t i = 0; i < mt_len(x); i++) {
        exact(a, mt_at(x, i));
        exact(b, mt_at(y, i));
        mpq_sub(a, a, b);
        mpq_mul(a, a, a);
        mpq_add(s, s, a);
    }
    double out = root(s);
    mpq_clears(s, a, b, NULL);
    return out;
}

/* ---- exact componentwise arithmetic -------------------------------------- */

static int64_t gcd(int64_t a, int64_t b) { while (b) { int64_t t = a % b; a = b; b = t; } return a < 0 ? -a : a; }

static mt_atom *ratio(int64_t num, int64_t den)
{
    int64_t g = gcd(num, den);
    num /= g;
    den /= g;
    if (den < 0) { num = -num; den = -den; }
    return den == 1 ? mt_num(num) : mt_rational(num, den);
}

typedef mt_atom *(*combine_fn)(int64_t, int64_t);
static mt_atom *plus(int64_t a, int64_t b) { return mt_num(a + b); }
static mt_atom *minus(int64_t a, int64_t b) { return mt_num(a - b); }
static mt_atom *times(int64_t a, int64_t b) { return mt_num(a * b); }
static mt_atom *over(int64_t a, int64_t b) { return ratio(a, b); }

static mt_atom *componentwise(const int64_t *x, const int64_t *y, size_t n, combine_fn f)
{
    mt_atom *kids[MOST];
    for (size_t i = 0; i < n; i++) kids[i] = f(x[i], y[i]);
    return mt_exprv(n, kids);
}

static mt_atom *filled(size_t n, mt_atom *value)
{
    mt_atom *kids[MOST];
    for (size_t i = 0; i < n; i++) kids[i] = mt_keep(value);
    mt_drop(value);
    return mt_exprv(n, kids);
}

static mt_atom *reals(size_t n, const double *xs)
{
    mt_atom *kids[MOST];
    for (size_t i = 0; i < n; i++) kids[i] = mt_real(xs[i]);
    return mt_exprv(n, kids);
}

/* What if-error answers: refused when C's precondition fails. A count is an
   integer at least zero; a component, or a value to fill with, a number. */
static mt_atom *verdict(bool holds) { return mt_sym(holds ? "accepted" : "refused"); }
static bool a_count(const mt_atom *n) { return mt_kind_of(n) == MT_INT && mt_int(n) >= 0; }
static mt_atom *guarded(mt_atom *goal) { return E("if-error", E("catch", goal), "refused", "accepted"); }

#define V2(a, b) reals(2, (const double[]){ a, b })

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_vector", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_vector")))));
    mt_atom *vs[32];
    size_t held = 0;
#define HOLD(x) (require("room to hold it", held < COUNT(vs)), vs[held++] = (x))

    /* Reductions, each rounded once. */
    mt_atom *x12 = HOLD(V2(1.0, 2.0)), *x34 = HOLD(V2(3.0, 4.0)), *e1 = HOLD(V2(1.0, 0.0)), *e2 = HOLD(V2(0.0, 1.0));
    mt_atom *none = HOLD(mt_unit());
    assert(answers_are(mt_eval(m, E("dot", mt_keep(x12), mt_keep(x34))), E(dot(x12, x34))) && "dot");
    assert(answers_are(mt_eval(m, E("dot", mt_keep(none), mt_keep(none))), E(dot(none, none))) && "the empty dot");
    assert(answers_are(mt_eval(m, E("dot", mt_keep(e1), mt_keep(e2))), E(dot(e1, e2))) && "orthogonal");
    assert(answers_are(mt_eval(m, E("norm", mt_keep(x34))), E(norm(x34))) && "norm");
    assert(answers_are(mt_eval(m, E("norm", mt_keep(e1))), E(norm(e1))) && "a unit norm");
    assert(answers_are(mt_eval(m, E("norm", mt_keep(none))), E(norm(none))) && "the empty norm");
    mt_atom *twice = HOLD(V2(2.0, 0.0)), *back = HOLD(V2(-2.0, 0.0)), *tilt = HOLD(V2(0.6, 0.8));
    assert(answers_are(mt_eval(m, E("cosine", mt_keep(e1), mt_keep(twice))), E(cosine(e1, twice))) && "the same direction");
    assert(answers_are(mt_eval(m, E("cosine", mt_keep(e1), mt_keep(e2))), E(cosine(e1, e2))) && "a right angle");
    assert(answers_are(mt_eval(m, E("cosine", mt_keep(e1), mt_keep(back))), E(cosine(e1, back))) && "the opposite direction");
    assert(answers_are(mt_eval(m, E("cosine-of-normalized", mt_keep(e1), mt_keep(e1))), E(dot(e1, e1))) && "the shortcut is dot");
    assert(answers_are(mt_eval(m, E("cosine-of-normalized", mt_keep(e1), mt_keep(e2))), E(dot(e1, e2))) && "on unit vectors");
    assert(answers_are(mt_eval(m, E("==", E("cosine-of-normalized", mt_keep(e1), mt_keep(tilt)), E("cosine", mt_keep(e1), mt_keep(tilt)))), E(B(dot(e1, tilt) == cosine(e1, tilt))))
           && "where it is the cosine");
    assert(answers_are(mt_eval(m, E("cosine-of-normalized", mt_keep(x34), mt_keep(x34))), E(dot(x34, x34))) && "and dot on a nonunit vector");
    assert(answers_are(mt_eval(m, E("cosine", mt_keep(x34), mt_keep(x34))), E(cosine(x34, x34))) && "where cosine is 1");

    /* Random vectors: C knows the size it asked for and that the length is one. */
    assert(answers_are(mt_eval(m, E("let", V("v"), E("with-seed", 17, E("random-normal-vector", 3)), E("size-atom", V("v")))), E((int64_t)3))
           && "a seeded random vector has the size asked");
    mt_atom *five = mt_one(mt_eval(m, E("with-seed", 17, E("random-normal-vector", 5))));
    require("a vector of five", five && mt_len(five) == 5);
    assert(answers_are(mt_eval(m, E("let", V("v"), E("with-seed", 17, E("random-normal-vector", 5)),
                                    E("<", E("abs-math", E("-", E("norm", V("v")), 1.0)), 0.000001))), E(B(fabs(norm(five) - 1.0) < 0.000001)))
           && "unit length within the rounding of its coordinates");
    mt_drop(five);
    mt_atom *four = mt_one(mt_eval(m, E("with-seed", 17, E("random-normal-vector", 4))));
    require("a vector of four", four && mt_len(four) == 4);
    assert(answers_are(mt_eval(m, E("let", V("v"), E("with-seed", 17, E("random-normal-vector", 4)),
                                    E("<", E("abs-math", E("-", E("cosine-of-normalized", V("v"), V("v")), 1.0)), 0.000001))), E(B(fabs(dot(four, four) - 1.0) < 0.000001)))
           && "so its dot with itself is 1");
    mt_drop(four);

    /* Exact componentwise arithmetic. */
    const int64_t i12[] = { 1, 2 }, i34[] = { 3, 4 }, i68[] = { 6, 8 }, i24[] = { 2, 4 }, three[] = { 3, 3 }, i46[] = { 4, 6 };
    assert(answers_are(mt_eval(m, E("vector-add", mt_array(2, i12), mt_array(2, i34))), E(componentwise(i12, i34, 2, plus))) && "add");
    assert(answers_are(mt_eval(m, E("vector-subtract", mt_array(2, i34), mt_array(2, i12))), E(componentwise(i34, i12, 2, minus))) && "subtract");
    assert(answers_are(mt_eval(m, E("vector-multiply", mt_array(2, i12), mt_array(2, i34))), E(componentwise(i12, i34, 2, times))) && "multiply");
    assert(answers_are(mt_eval(m, E("vector-divide", mt_array(2, i68), mt_array(2, i24))), E(componentwise(i68, i24, 2, over))) && "divide, exactly");
    assert(answers_are(mt_eval(m, E("vector-scale", mt_array(2, i12), 3)), E(componentwise(i12, three, 2, times))) && "scale");
    mt_atom *exact34 = HOLD(mt_array(2, i34));
    assert(answers_are(mt_eval(m, E("vector-normalize", mt_keep(exact34))), E(normalized(exact34))) && "normalize");
    mt_atom *from = HOLD(mt_array(2, i12)), *to = HOLD(mt_array(2, i46));
    assert(answers_are(mt_eval(m, E("vector-distance", mt_keep(from), mt_keep(to))), E(distance(from, to))) && "distance");
    assert(answers_are(mt_eval(m, E("vector-fill", 3, 7)), E(filled(3, mt_num(7)))) && "fill");
    assert(answers_are(mt_eval(m, E("vector-fill", 0, 7)), E(filled(0, mt_num(7)))) && "fill none");
    assert(answers_are(mt_eval(m, E("vector-normalize", mt_keep(none))), E(normalized(none))) && "normalize nothing");

    /* Exactness where floats would lose it. */
    mt_atom *cancel = HOLD(reals(3, (const double[]){ 18014398509481984.0, 1.0, -18014398509481984.0 }));
    mt_atom *ones = HOLD(E(1, 1, 1));
    assert(answers_are(mt_eval(m, E("dot", mt_keep(cancel), mt_keep(ones))), E(dot(cancel, ones))) && "2^54 + 1 - 2^54 keeps its 1");
    mt_atom *tiny = HOLD(reals(1, (const double[]){ 1e-300 }));
    assert(answers_are(mt_eval(m, E("norm", mt_keep(tiny))), E(norm(tiny))) && "a length whose square underflows");
    mt_atom *huge = HOLD(V2(1e308, 1e308));
    assert(answers_are(mt_eval(m, E("cosine", mt_keep(huge), mt_keep(huge))), E(cosine(huge, huge))) && "a cosine whose norms overflow");
    mt_atom *zero = HOLD(E(0, 0));
    assert(answers_are(mt_eval(m, E("isnan-math", E("cosine", mt_keep(zero), mt_keep(from)))), E(B(isnan(cosine(zero, from)))))
           && "a zero direction has no cosine");
    /* 1e400 overflows a double, so the original's component is infinity. */
    mt_atom *infinite = HOLD(reals(1, (const double[]){ HUGE_VAL }));
    assert(answers_are(mt_eval(m, E("isinf-math", E("norm", mt_keep(infinite)))), E(B(isinf(norm(infinite)))))
           && "an infinite length stays infinite");

    /* Zero draws normalize the accumulator; negative counts draw nothing. */
    assert(answers_are(mt_eval(m, E("random-normal-vector", 0, mt_keep(exact34))), E(normalized(exact34))) && "zero draws");
    assert(answers_are(mt_eval(m, E("random-normal-vector", -2)), E(normalized(none))) && "negative draws");

    /* Construction as a relation. */
    assert(answers_are(mt_eval(m, E("collapse", E("vector-fill", E("superpose", E(0, 2)), 7))), E(E(filled(0, mt_num(7)), filled(2, mt_num(7)))))
           && "fill over a stream of counts");
    assert(answers_are(mt_eval(m, E("let", V("recipe"),
                                    E("match", "&self", E("=", E("vector-fill", V("count"), V("value")), V("body")),
                                      E("quote", E("|->", E(V("count"), V("value")), V("body")))),
                                    E("let", V("constructor"), E("eval", V("recipe")), E(V("constructor"), 3, 7)))), E(filled(3, mt_num(7))))
           && "the library's own equation, as a function");
    mt_atom *third = ratio(1, 3);
    mt_atom *scaled[3];
    mt_ratio r = mt_ratio_of(third);
    for (size_t i = 0; i < 3; i++) scaled[i] = ratio(r.num * 3, r.den);
    mt_drop(third);
    assert(answers_are(mt_eval(m, E("let", V("q"), E("index-atom", E("vector-divide", E(1), E(3)), 0),
                                    E("vector-scale", E("vector-fill", 3, V("q")), 3))), E(mt_exprv(3, scaled)))
           && "a third, filled and scaled, stays exact");
    assert(answers_are(mt_eval(m, E("vector-fill", 2, -0.0)), E(filled(2, mt_real(-0.0)))) && "signed zeros keep their sign");

    /* Refusals, before anything is produced. */
    mt_atom *minus_one = HOLD(mt_num(-1)), *bad = HOLD(mt_sym("bad")), *half = HOLD(mt_real(1.5));
    assert(answers_are(mt_eval(m, guarded(E("vector-fill", mt_keep(minus_one), 7))), E(verdict(a_count(minus_one)))) && "a negative count");
    assert(answers_are(mt_eval(m, guarded(E("vector-fill", 0, mt_keep(bad)))), E(verdict(is_number(bad)))) && "a value that is no number");
    assert(answers_are(mt_eval(m, guarded(E("random-normal-vector", mt_keep(half)))), E(verdict(a_count(half)))) && "a fractional count");
    assert(answers_are(mt_eval(m, guarded(E("random-normal-vector", 0, E(mt_keep(bad))))), E(verdict(is_number(bad))))
           && "a component that is no number");
    mt_atom *first = mt_one(mt_eval(m, E("with-seed", 17, E("random-float", 0, 1))));
    mt_atom *after = mt_one(mt_eval(m, E("with-seed", 17, E("if-error", E("catch", E("random-normal-vector", 3,
                                                     E("quote", E(E("random-float", 0, 1))))), E("random-float", 0, 1), 0))));
    assert(answers_are(mt_eval(m, E("==", E("with-seed", 17, E("if-error", E("catch", E("random-normal-vector", 3,
                                                               E("quote", E(E("random-float", 0, 1))))), E("random-float", 0, 1), 0)),
                                    E("with-seed", 17, E("random-float", 0, 1)))), E(B(first && after && mt_eq(first, after))))
           && "a runnable component is refused without drawing");
    mt_drop(first);
    mt_drop(after);

    for (size_t i = 0; i < held; i++) mt_drop(vs[i]);
    mt_close(m);
    return 0;
}
#else
#include <stdio.h>

/* Without GMP's headers the program only says what it needs. */
int main(void)
{
    fputs("13-vector_lib.c needs GMP: install its development files, then build with\n"
          "cc 13-vector_lib.c $(pkg-config --cflags --libs cmetta gmp)\n", stderr);
    return 77;
}
#endif
