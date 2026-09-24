/* Purpose: lib_random, held against C. A sampler is a program held as an
 *   expression, which C reads apart with mt_at and rewrites. Randomness is an
 *   input: for a seed, C asks the engine for the same uniform stream its
 *   samplers draw from, (repeat K (random-float 0 1)) under that seed, and
 *   runs the library's own recipes over it in C, ported with GMP where the
 *   library is exact: Marsaglia and Tsang's gamma with shape boosting whose
 *   power correction stays separate until the scale is applied, Box-Muller
 *   with two draws, NumPy's log-difference beta, and the exact products and
 *   log-sums that keep extreme values representable (lib/_support/random.metta).
 *   A property any draw has, a bound or a sign or a class, C checks on the
 *   engine's own draw; a seed's determinism C checks by comparing two runs'
 *   answers; and a degenerate sampler is its parameter, which C computes.
 *   Each refusal is a parameter C's own check of that sampler rejects.
 * Assumes: GMP, found through pkg-config; exact_oracle.h rounds exact
 *   values once.
 * Guarantees: all sixty-eight claims of the original hold [tested: make
 *   twins; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define _XOPEN_SOURCE 700 /* M_PI and M_E are XSI's */
#define MT_SHORTHAND
#include "common.h"
#include "exact_oracle.h"

enum { STREAM = 64, MOST = 8 };

/* ---- the engine's stream, and the library's recipes over it ------------- */

typedef struct stream {
    double at[STREAM];
    size_t n, next;
} stream;

static stream stream_of(metta *m, int64_t seed)
{
    stream s = { .n = 0, .next = 0 };
    mt_atom *draws = mt_one(mt_eval(m, E("with-seed", seed, E("collapse", E("repeat", STREAM, E("random-float", 0, 1))))));
    require("the engine's stream", draws && mt_len(draws) == STREAM);
    for (size_t i = 0; i < STREAM; i++) s.at[s.n++] = mt_float(mt_at(draws, i));
    mt_drop(draws);
    return s;
}

static double draw(stream *s)
{
    require("enough of the stream", s->next < s->n);
    return s->at[s->next++];
}

/* log-math with base e is log(x) / log(e), as the engine's log/2 divides. */
static double ln(double x) { return log(x) / log(M_E); }

static double standard_normal(stream *s)
{
    double u = draw(s), v = draw(s);
    return cos(2 * M_PI * u) * sqrt(-2 * ln(v));
}

/* Marsaglia and Tsang's acceptance for d and c. */
static double accepted(stream *s, double d, double c)
{
    for (;;) {
        double z = standard_normal(s), root = 1.0 + c * z;
        if (!(root > 0.0)) continue;
        double v = root * root * root, u = draw(s), z2 = z * z;
        if (u < 1.0 - 0.0331 * z2 * z2 || ln(u) < 0.5 * z2 + d * (1.0 - v + ln(v))) return v;
    }
}

/* A gamma draw as factors and a power correction, the correction an exact
   rational: log(u) / shape for a boosted shape below one, else zero. */
typedef struct parts {
    double factor[2];
    size_t n;
    mpq_t correction;
} parts;

static void gamma_factors(stream *s, double shape, parts *p)
{
    if (shape == 1.0) {
        p->factor[0] = 0.0 - ln(draw(s));
        p->n = 1;
        return;
    }
    double d = shape - 1.0 / 3.0, c = 1.0 / 3.0 / sqrt(d);
    p->factor[0] = d;
    p->factor[1] = accepted(s, d, c);
    p->n = 2;
}

static void gamma_parts(stream *s, double shape, parts *p)
{
    mpq_init(p->correction);
    if (shape < 1.0) {
        gamma_factors(s, shape + 1.0, p);
        mpq_t divisor;
        mpq_init(divisor);
        mpq_set_d(p->correction, ln(draw(s)));
        mpq_set_d(divisor, shape);
        mpq_div(p->correction, p->correction, divisor);
        mpq_clear(divisor);
    } else
        gamma_factors(s, shape, p);
}

/* The exact product of doubles, and the exact sum of a correction and the
   factors' logarithms. */
static void product(mpq_t out, const double *f, size_t n)
{
    mpq_t x;
    mpq_init(x);
    mpq_set_ui(out, 1, 1);
    for (size_t i = 0; i < n; i++) mpq_set_d(x, f[i]), mpq_mul(out, out, x);
    mpq_clear(x);
}

static void log_product(mpq_t out, const double *f, size_t n, const mpq_t correction)
{
    mpq_t x;
    mpq_init(x);
    mpq_set(out, correction);
    for (size_t i = 0; i < n; i++) mpq_set_d(x, ln(f[i])), mpq_add(out, out, x);
    mpq_clear(x);
}

/* Ordinary powers multiply exactly; a power that is not normal goes through
   the exact log-sum instead, so a representable result stays representable. */
static double positive_value(const double *f, size_t n, const mpq_t correction)
{
    double power = exp(rounded(correction)), with[MOST];
    mpq_t exact_value;
    mpq_init(exact_value);
    double out;
    if (fpclassify(power) == FP_NORMAL) {
        with[0] = power;
        for (size_t i = 0; i < n; i++) with[i + 1] = f[i];
        product(exact_value, with, n + 1);
        out = rounded(exact_value);
    } else {
        log_product(exact_value, f, n, correction);
        out = exp(rounded(exact_value));
    }
    mpq_clear(exact_value);
    return out;
}

static double gamma_draw(stream *s, double shape, double scale)
{
    parts p;
    gamma_parts(s, shape, &p);
    double f[MOST] = { scale };
    for (size_t i = 0; i < p.n; i++) f[i + 1] = p.factor[i];
    double out = positive_value(f, p.n + 1, p.correction);
    mpq_clear(p.correction);
    return out;
}

static double beta_draw(stream *s, double alpha, double beta)
{
    parts a, b;
    gamma_parts(s, alpha, &a);
    gamma_parts(s, beta, &b);
    mpq_t x, y, sum;
    mpq_inits(x, y, sum, NULL);
    double out;
    if (mpq_equal(a.correction, b.correction)) {
        product(x, a.factor, a.n);
        product(y, b.factor, b.n);
        mpq_add(sum, x, y);
        mpq_div(x, x, sum);
        out = rounded(x);
    } else {
        log_product(x, a.factor, a.n, a.correction);
        log_product(y, b.factor, b.n, b.correction);
        mpq_sub(x, x, y);
        double delta = rounded(x), tail = exp(-fabs(delta));
        out = mpq_sgn(x) >= 0 ? 1.0 / (1.0 + tail) : tail / (1.0 + tail);
    }
    mpq_clears(x, y, sum, a.correction, b.correction, NULL);
    return out;
}

static double weibull_draw(stream *s, double scale, double shape)
{
    mpq_t correction, divisor;
    mpq_inits(correction, divisor, NULL);
    mpq_set_d(correction, ln(0.0 - ln(draw(s))));
    mpq_set_d(divisor, shape);
    mpq_div(correction, correction, divisor);
    double out = positive_value(&scale, 1, correction);
    mpq_clears(correction, divisor, NULL);
    return out;
}

/* ---- the samplers' parameter checks ------------------------------------- */

static bool finite(const mt_atom *x) { return (mt_kind_of(x) == MT_INT || mt_kind_of(x) == MT_FLOAT) && isfinite(mt_float(x)); }
static double num(const mt_atom *x) { return mt_float(x); }

typedef bool precondition(const mt_atom *const *p);
static bool normal_ok(const mt_atom *const *p) { return finite(p[0]) && finite(p[1]) && num(p[1]) >= 0; }
static bool uniform_ok(const mt_atom *const *p) { return finite(p[0]) && finite(p[1]) && num(p[0]) <= num(p[1]); }
static bool triangular_ok(const mt_atom *const *p)
{
    return finite(p[0]) && finite(p[1]) && finite(p[2]) && num(p[0]) <= num(p[2]) && num(p[2]) <= num(p[1]);
}
static bool positive_ok(const mt_atom *const *p) { return finite(p[0]) && num(p[0]) > 0; }
static bool two_positive_ok(const mt_atom *const *p) { return positive_ok(p) && positive_ok(p + 1); }
static bool probability_ok(const mt_atom *const *p) { return finite(p[0]) && num(p[0]) >= 0 && num(p[0]) <= 1; }

static bool sample_ok(const mt_atom *items, const mt_atom *count)
{
    return mt_kind_of(items) == MT_EXPR && mt_kind_of(count) == MT_INT && mt_int(count) >= 0 && (size_t)mt_int(count) <= mt_len(items);
}

static mt_atom *verdict(bool holds) { return S(holds ? "fine" : "refused"); }
static mt_answers *refusal(metta *m, mt_atom *goal) { return mt_eval(m, E("if-error", E("catch", goal), "refused", "fine")); }

static mt_atom *value_of(metta *m, mt_atom *goal)
{
    mt_atom *v = mt_one(mt_eval(m, goal));
    require("a value", v != NULL);
    return v;
}

/* A sampler's draw: its program evaluated once, under a seed or none. */
static mt_atom *drawn(mt_atom *sampler) { return E("let", V("drawn"), sampler, E("eval", V("drawn"))); }
static mt_atom *seeded(int64_t seed, mt_atom *goal) { return E("with-seed", seed, goal); }

static mt_atom *sorted(const mt_atom *items)
{
    mt_atom *kids[MOST];
    for (size_t i = 0; i < mt_len(items); i++) kids[i] = mt_keep(mt_at(items, i));
    qsort(kids, mt_len(items), sizeof *kids, mt_order);
    return mt_exprv(mt_len(items), kids);
}

static mt_atom *repeated(const mt_atom *x, size_t n)
{
    mt_atom *kids[MOST];
    for (size_t i = 0; i < n; i++) kids[i] = mt_keep(x);
    return mt_exprv(n, kids);
}

/* Two runs of a goal answer alike: C compares them. */
static bool alike(metta *m, mt_atom *goal)
{
    mt_atom *first = value_of(m, mt_keep(goal)), *second = value_of(m, goal);
    bool same = mt_eq(first, second);
    mt_drop(first);
    mt_drop(second);
    return same;
}

int main(void)
{
    metta *m = open_engine();
    require("import lib_random", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_random")))));

    /* A sampler is a program. */
    mt_atom *program = value_of(m, E("random-normal", 0, 1));
    check_answers("a program is an expression", mt_eval(m, E("let", V("sample"), E("random-normal", 0, 1), E("get-metatype", V("sample")))),
                  S(mt_kind_of(program) == MT_EXPR ? "Expression" : "Grounded"));
    check_answers("taking two numbers", mt_eval(m, E("get-type", "random-normal")), E("->", "Number", "Number", "Expression"));
    mt_atom *only = E(T("only")), *sum = E(E("+", 1, 2));
    check_answers("a singleton is its choice", mt_eval(m, drawn(E("random-choice", mt_keep(only)))), mt_keep(mt_at(only, 0)));
    check_answers("runnable data stays data", mt_eval(m, E("size-atom", drawn(E("random-choice", mt_keep(sum))))), (int64_t)mt_len(mt_at(sum, 0)));
    check_answers("no shuffle of nothing", mt_eval(m, E("random-shuffle!", mt_unit())), mt_unit());
    mt_atom *four = E(1, 1, 2, 3), *fours = E(1, 2, 3, 4);
    check_answers("a shuffle is a permutation", mt_eval(m, seeded(42, E("sort-atom", E("random-shuffle!", mt_keep(four))))), sorted(four));
    check("a seed replays a shuffle", alike(m, seeded(42, E("random-shuffle!", mt_keep(fours)))));
    check_answers("no sample of nothing", mt_eval(m, E("random-sample!", mt_unit(), 0)), mt_unit());
    check_answers("a full sample is a permutation", mt_eval(m, seeded(42, E("sort-atom", E("random-sample!", mt_keep(four), 4)))), sorted(four));
    mt_atom *x = E(T("x"));
    check_answers("repeat owns repetition",
                  mt_eval(m, E("let", V("choice"), E("random-choice", mt_keep(x)), E("collapse", E("repeat", 3, V("choice"))))),
                  repeated(mt_at(x, 0), 3));
    mt_atom *five = E(1, 2, 3, 4, 5), *xx = E(T("x"), T("x"));
    check_answers("a sample's size", mt_eval(m, seeded(42, E("size-atom", E("random-sample!", mt_keep(five), 3)))), (int64_t)3);
    check_answers("equal values are separate occurrences", mt_eval(m, seeded(42, E("random-sample!", mt_keep(xx), 2))), mt_keep(xx));
    check("a seed replays a choice", alike(m, seeded(9, drawn(E("random-choice", E(1, 2, 3, 4))))));
    mt_atom *var = E(V("x")), *vars = E(V("x"), V("x")), *error = E(E("Error", "data", "code"));
    /* A singleton's every choice is its element, and a full sample is a
       permutation, so C's expectations are the population's own atoms. */
    mt_atom *three_x = repeated(mt_at(var, 0), 3), *xs = E(V("x"), V("x"), V("x")), *vars_sorted = sorted(vars), *error_sorted = sorted(error);
    check_answers("a variable keeps its identity",
                  mt_eval(m, E("let", V("choice"), E("random-choice", mt_keep(var)),
                               E("==", E("map-atom", E(0, 1, 2), V("i"), E("eval", V("choice"))), E("quote", mt_keep(xs))))),
                  B(mt_eq(three_x, xs)));
    check_answers("in a sample too",
                  mt_eval(m, E("let", V("sample"), seeded(42, E("random-sample!", mt_keep(vars), 2)), E("==", V("sample"), E("quote", mt_keep(vars))))),
                  B(mt_eq(vars_sorted, vars)));
    check_answers("an Error expression is data",
                  mt_eval(m, E("let", V("sample"), E("random-sample!", mt_keep(error), 1), E("==", V("sample"), E("quote", mt_keep(error))))),
                  B(mt_eq(error_sorted, error)));

    /* Degenerate samplers are their parameters; repeat 0 draws nothing. */
    check_answers("no samples", mt_eval(m, E("let", V("sample"), E("random-normal", 0, 1), E("collapse", E("repeat", 0, V("sample"))))), mt_unit());
    mt_atom *fours_f = E(4.0, 4.0, 4.0);
    check_answers("a zero-width uniform", mt_eval(m, E("let", V("sample"), E("random-uniform", 4, 4), E("collapse", E("repeat", 3, V("sample"))))),
                  mt_keep(fours_f));
    check_answers("a zero-deviation normal", mt_eval(m, drawn(E("random-normal", 7, 0))), (double)7);
    check_answers("a zero-deviation lognormal", mt_eval(m, drawn(E("random-lognormal", 0, 0))), exp(0.0));
    check_answers("a point triangle", mt_eval(m, drawn(E("random-triangular", 3, 3, 3))), (double)3);
    check_answers("never", mt_eval(m, E("let", V("sample"), E("random-bernoulli", 0), E("collapse", E("repeat", 2, V("sample"))))),
                  E(B(false), B(false)));
    check_answers("always", mt_eval(m, E("let", V("sample"), E("random-bernoulli", 1), E("collapse", E("repeat", 2, V("sample"))))),
                  E(B(true), B(true)));
    check_answers("a seeded stream's length",
                  mt_eval(m, E("let", V("sample"), E("random-normal", 0, 1), seeded(42, E("size-atom", E("collapse", E("repeat", 5, V("sample"))))))),
                  (int64_t)5);
    check("a seed replays a stream", alike(m, E("let", V("sample"), E("random-normal", 0, 1), seeded(42, E("collapse", E("repeat", 4, V("sample")))))));
    mt_atom *first = value_of(m, E("let", V("sample"), E("random-normal", 0, 1), seeded(42, E("once", E("repeat", 100, V("sample"))))));
    mt_atom *one = value_of(m, E("let", V("sample"), E("random-normal", 0, 1), seeded(42, E("eval", V("sample")))));
    check("the first of a stream is one draw", mt_eq(first, one));
    check_answers("an exact mean", mt_eval(m, drawn(E("random-normal", E("math-rational", 1, 2), 0))), 0.5);

    /* Properties of any draw, checked in C on the engine's draw under seed 11. */
    mt_atom *u = value_of(m, seeded(11, drawn(E("random-uniform", -4, 9))));
    check("a uniform draw lies within its bounds", mt_float(u) >= -4 && mt_float(u) <= 9);
    mt_atom *n = value_of(m, seeded(11, drawn(E("random-normal", 0, 1))));
    check("a normal draw is a normal double", fpclassify(mt_float(n)) == FP_NORMAL);
    static const struct {
        const char *claim, *sampler;
        double p[2];
        size_t count;
        double at_least;
        bool strict;
    } positive[] = {
        { "a lognormal draw is positive", "random-lognormal", { 0, 1 }, 2, 0, true },
        { "an exponential draw is positive", "random-exponential", { 2 }, 1, 0, true },
        { "a gamma draw is positive", "random-gamma", { 2, 3 }, 2, 0, true },
        { "a Pareto draw is at least one", "random-pareto", { 3 }, 1, 1, false },
        { "a Weibull draw is positive", "random-weibull", { 2, 3 }, 2, 0, true },
    };
    for (size_t i = 0; i < sizeof positive / sizeof *positive; i++) {
        mt_atom *sampler = positive[i].count == 1 ? E(positive[i].sampler, positive[i].p[0]) : E(positive[i].sampler, positive[i].p[0], positive[i].p[1]);
        mt_atom *d = value_of(m, seeded(11, drawn(sampler)));
        check(positive[i].claim, positive[i].strict ? mt_float(d) > positive[i].at_least : mt_float(d) >= positive[i].at_least);
        mt_drop(d);
    }
    mt_atom *t = value_of(m, seeded(11, drawn(E("random-triangular", -4, 9, 2))));
    check("a triangular draw lies within its bounds", mt_float(t) >= -4 && mt_float(t) <= 9);
    mt_atom *b = value_of(m, seeded(11, drawn(E("random-beta", 2, 5))));
    check("a beta draw lies strictly inside the unit interval", mt_float(b) > 0 && mt_float(b) < 1);
    mt_atom *coin = value_of(m, seeded(11, drawn(E("random-bernoulli", 0.25))));
    check("a Bernoulli draw is a Bool", mt_kind_of(coin) == MT_BOOL);

    /* Values that depend on the draws: C runs the library's recipes over the
       engine's own stream for the seed. */
    stream s42 = stream_of(m, 42), s2 = stream_of(m, 2);
    stream s = s42;
    check_answers("gamma at an enormous shape", mt_eval(m, seeded(42, drawn(E("random-gamma", 1.0e308, 1)))), gamma_draw(&s, 1.0e308, 1));
    s = s42;
    check_answers("beta at enormous shapes", mt_eval(m, seeded(42, drawn(E("random-beta", 1.0e308, 1.0e308)))), beta_draw(&s, 1.0e308, 1.0e308));
    s = s2;
    check_answers("a tiny shape underflows", mt_eval(m, seeded(2, drawn(E("random-gamma", 0.001, 1)))), gamma_draw(&s, 0.001, 1));
    s = s2;
    double rescued = gamma_draw(&s, 0.001, 1.0e300);
    check_answers("and a large scale rescues it", mt_eval(m, seeded(2, E(">", drawn(E("random-gamma", 0.001, 1.0e300)), 0))), B(rescued > 0));
    s = s42;
    check_answers("Weibull at enormous parameters", mt_eval(m, seeded(42, drawn(E("random-weibull", 1.0e308, 1.0e308)))),
                  weibull_draw(&s, 1.0e308, 1.0e308));
    check_answers("a lognormal overflows to infinity", mt_eval(m, E("math-class", drawn(E("random-lognormal", 1000, 0)))),
                  S(isinf(exp(1000.0)) ? "infinite" : "normal"));
    check_answers("and underflows to zero", mt_eval(m, drawn(E("random-lognormal", -1000, 0))), exp(-1000.0));

    /* Programs are data: stored, read back, rewritten, rebuilt. */
    mt_atom *twelve = value_of(m, E("random-normal", 12, 0));
    require("store the sampler", mt_add(m, E("saved-sampler", twelve)));
    check_answers("a stored sampler draws", mt_eval(m, E("let", V("sample"), E("match", "&self", E("saved-sampler", V("code")), V("code")), E("eval", V("sample")))),
                  (double)12);
    mt_atom *uniform = value_of(m, E("random-uniform", 2, 10));
    require("an interpolation of an entropy between two bounds", mt_len(uniform) == 4);
    double r = 0.25, low = mt_float(mt_at(uniform, 2)), high = mt_float(mt_at(uniform, 3));
    mt_atom *rewritten = E(mt_keep(mt_at(uniform, 0)), r, mt_keep(mt_at(uniform, 2)), mt_keep(mt_at(uniform, 3)));
    mpq_t exact_r, lo, hi, dot;
    mpq_inits(exact_r, lo, hi, dot, NULL);
    mpq_set_d(exact_r, r), mpq_set_d(lo, low), mpq_set_d(hi, high);
    mpq_sub(hi, hi, lo), mpq_mul(hi, hi, exact_r), mpq_add(dot, lo, hi);
    check_answers("a rewritten entropy", mt_eval(m, E("eval", mt_keep(rewritten))), rounded(dot));
    mpq_clears(exact_r, lo, hi, dot, NULL);
    mt_atom *make = NULL;
    mt_rows (row, mt_match(m, E("=", E("random-normal", V("mean"), V("deviation")), V("body")))) {
        mt_drop(make);
        make = E("|->", E(mt_keep(mt_bound(row, "mean")), mt_keep(mt_bound(row, "deviation"))), mt_keep(mt_bound(row, "body")));
    }
    require("random-normal is an equation", make != NULL);
    mt_atom *constructor = value_of(m, make);
    check_answers("a rebuilt constructor", mt_eval(m, E("let", V("sample"), E(mt_keep(constructor), 9, 0), E("eval", V("sample")))), (double)9);
    check_answers("one program per alternative", mt_eval(m, E("collapse", E("let", V("sample"), E("random-normal", E("superpose", E(1, 2)), 0), E("eval", V("sample"))))),
                  E((double)1, (double)2));
    mt_atom *ab = E("a", "b");
    check_answers("a superposed program repeats every answer",
                  mt_eval(m, E("let", V("sample"), E("quote", E("superpose", mt_keep(ab))), E("collapse", E("repeat", 2, V("sample"))))),
                  E("a", "b", "a", "b"));
    check_answers("programs compose",
                  mt_eval(m, E("let*", E(E(V("a"), E("random-normal", 2, 0)), E(V("b"), E("random-uniform", 3, 3)),
                                         E(V("sum"), E("quote", E("+", E("eval", V("a")), E("eval", V("b")))))),
                               E("eval", V("sum")))),
                  (double)2 + (double)3);

    /* Refusals: each sampler's own check. */
    mt_atom *text = T("text"), *pair = E(1, 2), *single = E(1);
    mt_atom *nothing = mt_unit();
    check_answers("no choice from nothing", refusal(m, E("random-choice", mt_keep(nothing))), verdict(mt_len(nothing) > 0));
    check_answers("a population is a collection", refusal(m, E("random-shuffle!", mt_keep(text))), verdict(mt_kind_of(text) == MT_EXPR));
    static const struct {
        const char *claim;
        int count_kind;
        double count;
        int pop;
    } counts[] = { { "a count past the population", 0, 1, 0 }, { "and past it again", 0, 3, 2 },
                   { "a negative count", 0, -1, 1 },          { "a fractional count", 1, 1.0, 1 } };
    mt_atom *pops[] = { mt_unit(), mt_keep(single), mt_keep(pair) };
    for (size_t i = 0; i < 4; i++) {
        mt_atom *count = counts[i].count_kind ? mt_real(counts[i].count) : mt_num((int64_t)counts[i].count);
        const mt_atom *population = pops[counts[i].pop == 2 ? 2 : counts[i].pop == 1 ? 1 : 0];
        check_answers(counts[i].claim, refusal(m, E("random-sample!", mt_keep(population), mt_keep(count))), verdict(sample_ok(population, count)));
        mt_drop(count);
    }
    for (size_t i = 0; i < 3; i++) mt_drop(pops[i]);
    static const struct {
        const char *claim, *sampler;
        double p[3];
        size_t n;
        precondition *ok;
    } bad[] = {
        { "a negative deviation", "random-normal", { 0, -1 }, 2, normal_ok },
        { "bounds out of order", "random-uniform", { 2, 1 }, 2, uniform_ok },
        { "a mode outside", "random-triangular", { 0, 1, 2 }, 3, triangular_ok },
        { "a zero rate", "random-exponential", { 0 }, 1, positive_ok },
        { "a negative shape", "random-gamma", { -1, 2 }, 2, two_positive_ok },
        { "a zero beta", "random-beta", { 2, 0 }, 2, two_positive_ok },
        { "a probability past one", "random-bernoulli", { 1.1 }, 1, probability_ok },
        { "a zero Pareto shape", "random-pareto", { 0 }, 1, positive_ok },
        { "a zero Weibull scale", "random-weibull", { 0, 1 }, 2, two_positive_ok },
    };
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
        mt_atom *p[3];
        for (size_t k = 0; k < bad[i].n; k++)
            p[k] = bad[i].p[k] == trunc(bad[i].p[k]) ? mt_num((int64_t)bad[i].p[k]) : mt_real(bad[i].p[k]);
        mt_atom *goal = bad[i].n == 1 ? E(bad[i].sampler, mt_keep(p[0])) : bad[i].n == 2 ? E(bad[i].sampler, mt_keep(p[0]), mt_keep(p[1]))
                                                                                          : E(bad[i].sampler, mt_keep(p[0]), mt_keep(p[1]), mt_keep(p[2]));
        check_answers(bad[i].claim, refusal(m, goal), verdict(bad[i].ok((const mt_atom *const *)p)));
        for (size_t k = 0; k < bad[i].n; k++) mt_drop(p[k]);
    }
    mt_atom *mean_x[] = { T("x"), mt_num(1) }, *infinite[] = { mt_real(INFINITY), mt_num(1) }, *minus[] = { mt_num(0), mt_num(-1) };
    check_answers("a text mean", refusal(m, E("random-normal", mt_keep(mean_x[0]), mt_keep(mean_x[1]))), verdict(normal_ok((const mt_atom *const *)mean_x)));
    check_answers("an infinite mean", refusal(m, E("random-normal", E("math-real", "inf", mt_unit()), 1)), verdict(normal_ok((const mt_atom *const *)infinite)));
    check_answers("refused before any draw",
                  refusal(m, E("let", V("sample"), E("random-normal", 0, -1), E("repeat", 0, V("sample")))), verdict(normal_ok((const mt_atom *const *)minus)));

    mt_atom *held[] = { program, only, sum, three_x, xs, vars_sorted, error_sorted, nothing, four, fours, x, five, xx, var, vars, error, fours_f, first, one, u, n, t, b, coin, uniform, rewritten,
                        constructor, ab, text, pair, single, mean_x[0], mean_x[1], infinite[0], infinite[1], minus[0], minus[1] };
    for (size_t i = 0; i < sizeof held / sizeof *held; i++) mt_drop(held[i]);
    return done(m);
}
