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
 * Assumes: GMP, found through pkg-config; the rounding and the root come
 *   from exact_oracle.h, which 35-math_lib shares.
 * Guarantees: all forty-three claims of the original hold [tested: make
 *   twins; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"
#include "exact_oracle.h"

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

static mt_atom *ints(size_t n, const int64_t *xs)
{
    mt_atom *kids[MOST];
    for (size_t i = 0; i < n; i++) kids[i] = mt_num(xs[i]);
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
    metta *m = open_engine();
    require("import lib_vector", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_vector")))));
    mt_atom *vs[32];
    size_t held = 0;
#define HOLD(x) (require("room to hold it", held < COUNT(vs)), vs[held++] = (x))

    /* Reductions, each rounded once. */
    mt_atom *x12 = HOLD(V2(1.0, 2.0)), *x34 = HOLD(V2(3.0, 4.0)), *e1 = HOLD(V2(1.0, 0.0)), *e2 = HOLD(V2(0.0, 1.0));
    mt_atom *none = HOLD(mt_unit());
    check_answers("dot", mt_eval(m, E("dot", mt_keep(x12), mt_keep(x34))), dot(x12, x34));
    check_answers("the empty dot", mt_eval(m, E("dot", mt_keep(none), mt_keep(none))), dot(none, none));
    check_answers("orthogonal", mt_eval(m, E("dot", mt_keep(e1), mt_keep(e2))), dot(e1, e2));
    check_answers("norm", mt_eval(m, E("norm", mt_keep(x34))), norm(x34));
    check_answers("a unit norm", mt_eval(m, E("norm", mt_keep(e1))), norm(e1));
    check_answers("the empty norm", mt_eval(m, E("norm", mt_keep(none))), norm(none));
    mt_atom *twice = HOLD(V2(2.0, 0.0)), *back = HOLD(V2(-2.0, 0.0)), *tilt = HOLD(V2(0.6, 0.8));
    check_answers("the same direction", mt_eval(m, E("cosine", mt_keep(e1), mt_keep(twice))), cosine(e1, twice));
    check_answers("a right angle", mt_eval(m, E("cosine", mt_keep(e1), mt_keep(e2))), cosine(e1, e2));
    check_answers("the opposite direction", mt_eval(m, E("cosine", mt_keep(e1), mt_keep(back))), cosine(e1, back));
    check_answers("the shortcut is dot", mt_eval(m, E("cosine-of-normalized", mt_keep(e1), mt_keep(e1))), dot(e1, e1));
    check_answers("on unit vectors", mt_eval(m, E("cosine-of-normalized", mt_keep(e1), mt_keep(e2))), dot(e1, e2));
    check_answers("where it is the cosine",
                  mt_eval(m, E("==", E("cosine-of-normalized", mt_keep(e1), mt_keep(tilt)), E("cosine", mt_keep(e1), mt_keep(tilt)))),
                  B(dot(e1, tilt) == cosine(e1, tilt)));
    check_answers("and dot on a nonunit vector", mt_eval(m, E("cosine-of-normalized", mt_keep(x34), mt_keep(x34))), dot(x34, x34));
    check_answers("where cosine is 1", mt_eval(m, E("cosine", mt_keep(x34), mt_keep(x34))), cosine(x34, x34));

    /* Random vectors: C knows the size it asked for and that the length is one. */
    check_answers("a seeded random vector has the size asked",
                  mt_eval(m, E("let", V("v"), E("with-seed", 17, E("random-normal-vector", 3)), E("size-atom", V("v")))),
                  (int64_t)3);
    mt_atom *five = mt_one(mt_eval(m, E("with-seed", 17, E("random-normal-vector", 5))));
    require("a vector of five", five && mt_len(five) == 5);
    check_answers("unit length within the rounding of its coordinates",
                  mt_eval(m, E("let", V("v"), E("with-seed", 17, E("random-normal-vector", 5)),
                               E("<", E("abs-math", E("-", E("norm", V("v")), 1.0)), 0.000001))),
                  B(fabs(norm(five) - 1.0) < 0.000001));
    mt_drop(five);
    mt_atom *four = mt_one(mt_eval(m, E("with-seed", 17, E("random-normal-vector", 4))));
    require("a vector of four", four && mt_len(four) == 4);
    check_answers("so its dot with itself is 1",
                  mt_eval(m, E("let", V("v"), E("with-seed", 17, E("random-normal-vector", 4)),
                               E("<", E("abs-math", E("-", E("cosine-of-normalized", V("v"), V("v")), 1.0)), 0.000001))),
                  B(fabs(dot(four, four) - 1.0) < 0.000001));
    mt_drop(four);

    /* Exact componentwise arithmetic. */
    const int64_t i12[] = { 1, 2 }, i34[] = { 3, 4 }, i68[] = { 6, 8 }, i24[] = { 2, 4 }, three[] = { 3, 3 }, i46[] = { 4, 6 };
    check_answers("add", mt_eval(m, E("vector-add", ints(2, i12), ints(2, i34))), componentwise(i12, i34, 2, plus));
    check_answers("subtract", mt_eval(m, E("vector-subtract", ints(2, i34), ints(2, i12))), componentwise(i34, i12, 2, minus));
    check_answers("multiply", mt_eval(m, E("vector-multiply", ints(2, i12), ints(2, i34))), componentwise(i12, i34, 2, times));
    check_answers("divide, exactly", mt_eval(m, E("vector-divide", ints(2, i68), ints(2, i24))), componentwise(i68, i24, 2, over));
    check_answers("scale", mt_eval(m, E("vector-scale", ints(2, i12), 3)), componentwise(i12, three, 2, times));
    mt_atom *exact34 = HOLD(ints(2, i34));
    check_answers("normalize", mt_eval(m, E("vector-normalize", mt_keep(exact34))), normalized(exact34));
    mt_atom *from = HOLD(ints(2, i12)), *to = HOLD(ints(2, i46));
    check_answers("distance", mt_eval(m, E("vector-distance", mt_keep(from), mt_keep(to))), distance(from, to));
    check_answers("fill", mt_eval(m, E("vector-fill", 3, 7)), filled(3, mt_num(7)));
    check_answers("fill none", mt_eval(m, E("vector-fill", 0, 7)), filled(0, mt_num(7)));
    check_answers("normalize nothing", mt_eval(m, E("vector-normalize", mt_keep(none))), normalized(none));

    /* Exactness where floats would lose it. */
    mt_atom *cancel = HOLD(reals(3, (const double[]){ 18014398509481984.0, 1.0, -18014398509481984.0 }));
    mt_atom *ones = HOLD(ints(3, (const int64_t[]){ 1, 1, 1 }));
    check_answers("2^54 + 1 - 2^54 keeps its 1", mt_eval(m, E("dot", mt_keep(cancel), mt_keep(ones))), dot(cancel, ones));
    mt_atom *tiny = HOLD(reals(1, (const double[]){ 1e-300 }));
    check_answers("a length whose square underflows", mt_eval(m, E("norm", mt_keep(tiny))), norm(tiny));
    mt_atom *huge = HOLD(V2(1e308, 1e308));
    check_answers("a cosine whose norms overflow", mt_eval(m, E("cosine", mt_keep(huge), mt_keep(huge))), cosine(huge, huge));
    mt_atom *zero = HOLD(ints(2, (const int64_t[]){ 0, 0 }));
    check_answers("a zero direction has no cosine",
                  mt_eval(m, E("isnan-math", E("cosine", mt_keep(zero), mt_keep(from)))), B(isnan(cosine(zero, from))));
    /* 1e400 overflows a double, so the original's component is infinity. */
    mt_atom *infinite = HOLD(reals(1, (const double[]){ HUGE_VAL }));
    check_answers("an infinite length stays infinite", mt_eval(m, E("isinf-math", E("norm", mt_keep(infinite)))),
                  B(isinf(norm(infinite))));

    /* Zero draws normalize the accumulator; negative counts draw nothing. */
    check_answers("zero draws", mt_eval(m, E("random-normal-vector", 0, mt_keep(exact34))), normalized(exact34));
    check_answers("negative draws", mt_eval(m, E("random-normal-vector", -2)), normalized(none));

    /* Construction as a relation. */
    check_answers("fill over a stream of counts", mt_eval(m, E("collapse", E("vector-fill", E("superpose", E(0, 2)), 7))),
                  E(filled(0, mt_num(7)), filled(2, mt_num(7))));
    check_answers("the library's own equation, as a function",
                  mt_eval(m, E("let", V("recipe"),
                               E("match", "&self", E("=", E("vector-fill", V("count"), V("value")), V("body")),
                                 E("quote", E("|->", E(V("count"), V("value")), V("body")))),
                               E("let", V("constructor"), E("eval", V("recipe")), E(V("constructor"), 3, 7)))),
                  filled(3, mt_num(7)));
    mt_atom *third = ratio(1, 3);
    mt_atom *scaled[3];
    mt_ratio r = mt_ratio_of(third);
    for (size_t i = 0; i < 3; i++) scaled[i] = ratio(r.num * 3, r.den);
    mt_drop(third);
    check_answers("a third, filled and scaled, stays exact",
                  mt_eval(m, E("let", V("q"), E("index-atom", E("vector-divide", E(1), E(3)), 0),
                               E("vector-scale", E("vector-fill", 3, V("q")), 3))),
                  mt_exprv(3, scaled));
    check_answers("signed zeros keep their sign", mt_eval(m, E("vector-fill", 2, -0.0)), filled(2, mt_real(-0.0)));

    /* Refusals, before anything is produced. */
    mt_atom *minus_one = HOLD(mt_num(-1)), *bad = HOLD(mt_sym("bad")), *half = HOLD(mt_real(1.5));
    check_answers("a negative count", mt_eval(m, guarded(E("vector-fill", mt_keep(minus_one), 7))), verdict(a_count(minus_one)));
    check_answers("a value that is no number", mt_eval(m, guarded(E("vector-fill", 0, mt_keep(bad)))), verdict(is_number(bad)));
    check_answers("a fractional count", mt_eval(m, guarded(E("random-normal-vector", mt_keep(half)))), verdict(a_count(half)));
    check_answers("a component that is no number", mt_eval(m, guarded(E("random-normal-vector", 0, E(mt_keep(bad))))),
                  verdict(is_number(bad)));
    mt_atom *first = mt_one(mt_eval(m, E("with-seed", 17, E("random-float", 0, 1))));
    mt_atom *after = mt_one(mt_eval(m, E("with-seed", 17, E("if-error", E("catch", E("random-normal-vector", 3,
                                                     E("quote", E(E("random-float", 0, 1))))), E("random-float", 0, 1), 0))));
    check_answers("a runnable component is refused without drawing",
                  mt_eval(m, E("==", E("with-seed", 17, E("if-error", E("catch", E("random-normal-vector", 3,
                                                          E("quote", E(E("random-float", 0, 1))))), E("random-float", 0, 1), 0)),
                               E("with-seed", 17, E("random-float", 0, 1)))),
                  B(first && after && mt_eq(first, after)));
    mt_drop(first);
    mt_drop(after);

    for (size_t i = 0; i < held; i++) mt_drop(vs[i]);
    return done(m);
}
