/* Purpose: lib_statistics, held against statistics C computes itself over
 *   GMP rationals. A sample is its observations read exactly, a float as the
 *   dyadic rational it is, with a note of whether any was a float; every
 *   moment is exact until one final rounding, and only a sample holding a
 *   float rounds at all. Quantiles interpolate at CPython's positions, the
 *   geometric mean sums binary exponents apart from mantissa logs as CPython
 *   does, ranks average their ties, and the mode groups terms by identity.
 *   Every refusal is a C precondition the same function checks: no data, an
 *   observation that is no finite number, a sample no larger than its
 *   degrees of freedom, and the like. The two recipe claims read the
 *   variance's equation back out of the space with mt_match and apply it.
 * Guarantees: all seventy-eight claims of the original hold [tested: make
 *   twins; commit=WORKTREE].
 */
#define _XOPEN_SOURCE 700
#define MT_SHORTHAND
#include "common.h"
#include "exact_oracle.h"

/* A sample: the observations exactly, and whether one was a float. */
typedef struct sample {
    size_t n;
    bool floating;
    mpq_t *v;
} sample;

static void release(sample *s)
{
    for (size_t i = 0; i < s->n; i++) mpq_clear(s->v[i]);
    free(s->v);
    s->v = NULL;
    s->n = 0;
}

/* The observations of `data`; false, holding nothing, when one is not a
   finite number. */
static bool read_sample(const mt_atom *data, sample *s)
{
    s->n = 0;
    s->floating = false;
    s->v = NULL;
    if (mt_kind_of(data) != MT_EXPR) return false;
    s->v = malloc((mt_len(data) + 1) * sizeof *s->v);
    require("room for the sample", s->v != NULL);
    for (size_t i = 0; i < mt_len(data); i++) {
        const mt_atom *x = mt_at(data, i);
        mt_kind kind = mt_kind_of(x);
        bool number = kind == MT_INT || kind == MT_BIGINT || kind == MT_RATIONAL || kind == MT_BIGRATIONAL || kind == MT_FLOAT;
        if (!number || (kind == MT_FLOAT && !isfinite(mt_float(x)))) {
            release(s);
            return false;
        }
        mpq_init(s->v[s->n]);
        exact(s->v[s->n++], x);
        s->floating |= kind == MT_FLOAT;
    }
    return true;
}

/* What a statistic answers: rounded once when the sample held a float. */
static mt_atom *result(bool floating, const mpq_t q) { return floating ? mt_real(rounded(q)) : rational_of(q); }

static void sum_of(const sample *s, mpq_t out)
{
    mpq_set_ui(out, 0, 1);
    for (size_t i = 0; i < s->n; i++) mpq_add(out, out, s->v[i]);
}

static mt_atom *sum(const mt_atom *data)
{
    sample s;
    if (!read_sample(data, &s)) return NULL;
    mpq_t total;
    mpq_init(total);
    sum_of(&s, total);
    mt_atom *out = result(s.floating, total);
    mpq_clear(total);
    release(&s);
    return out;
}

static void mean_of(const sample *s, mpq_t out)
{
    mpq_t n;
    mpq_init(n);
    sum_of(s, out);
    mpq_set_ui(n, s->n, 1);
    mpq_div(out, out, n);
    mpq_clear(n);
}

static mt_atom *mean(const mt_atom *data)
{
    sample s;
    if (!read_sample(data, &s)) return NULL;
    mt_atom *out = NULL;
    if (s.n > 0) {
        mpq_t m;
        mpq_init(m);
        mean_of(&s, m);
        out = result(s.floating, m);
        mpq_clear(m);
    }
    release(&s);
    return out;
}

/* A nonempty sample of nonnegative observations, as both of the means below
   need; false, holding nothing, otherwise. */
static bool nonnegative_sample(const mt_atom *data, sample *s)
{
    if (!read_sample(data, s)) return false;
    bool fine = s->n > 0;
    for (size_t i = 0; i < s->n; i++) fine &= mpq_sgn(s->v[i]) >= 0;
    if (!fine) release(s);
    return fine;
}

static bool has_zero(const sample *s)
{
    for (size_t i = 0; i < s->n; i++)
        if (mpq_sgn(s->v[i]) == 0) return true;
    return false;
}

/* n over the sum of reciprocals; zero when an observation is. */
static mt_atom *harmonic_mean(const mt_atom *data)
{
    sample s;
    if (!nonnegative_sample(data, &s)) return NULL;
    mpq_t h, r;
    mpq_inits(h, r, NULL);
    if (!has_zero(&s)) {
        for (size_t i = 0; i < s.n; i++) {
            mpq_inv(r, s.v[i]);
            mpq_add(h, h, r);
        }
        mpq_set_ui(r, s.n, 1);
        mpq_div(h, r, h);
    }
    mt_atom *out = result(s.floating, h);
    mpq_clears(h, r, NULL);
    release(&s);
    return out;
}

/* log-math with base e, as the engine computes it: log(x) / log(e). */
static double natural_log(double x) { return log(x) / log(M_E); }

static long floor_div(long a, long b) { return a / b - (a % b != 0 && (a < 0) != (b < 0)); }

/* The geometric mean without a product that could overflow: each
   observation's binary exponent is summed exactly and apart from the log of
   its mantissa, the exponents' whole share of n becomes a power of two, and
   the estimate is held between the smallest and largest observation before
   its one rounding [source: https://github.com/python/cpython/blob/ebf955df7a89ed0c7968f79faec1de49f61ed7cb/Lib/statistics.py#L224-L261,
   which lib/_support/statistics.metta statistics-log-parts follows]. */
static mt_atom *geometric_mean(const mt_atom *data)
{
    sample s;
    if (!nonnegative_sample(data, &s)) return NULL;
    if (has_zero(&s)) {
        release(&s);
        return mt_real(0.0);
    }
    long exponent = 0;
    mpq_t logs, scaled, share, low, high;
    mpq_inits(logs, scaled, share, low, high, NULL);
    mpq_set(low, s.v[0]);
    mpq_set(high, s.v[0]);
    for (size_t i = 0; i < s.n; i++) {
        long shift = top_bit(mpq_numref(s.v[i])) - top_bit(mpq_denref(s.v[i]));
        if (shift >= 0) mpq_div_2exp(scaled, s.v[i], (mp_bitcnt_t)shift);
        else mpq_mul_2exp(scaled, s.v[i], (mp_bitcnt_t)-shift);
        mpq_set_d(share, natural_log(rounded(scaled)));
        mpq_add(logs, logs, share);
        exponent += shift;
        if (mpq_cmp(s.v[i], low) < 0) mpq_set(low, s.v[i]);
        if (mpq_cmp(s.v[i], high) > 0) mpq_set(high, s.v[i]);
    }
    long whole = floor_div(exponent, (long)s.n), rest = exponent - whole * (long)s.n;
    mpq_set_d(share, natural_log(2));
    mpq_set_si(scaled, rest, 1);
    mpq_mul(share, share, scaled);
    mpq_add(logs, logs, share);
    mpq_set_ui(scaled, s.n, 1);
    mpq_div(logs, logs, scaled);
    mpq_set_d(scaled, exp(rounded(logs)));
    if (whole >= 0) mpq_mul_2exp(scaled, scaled, (mp_bitcnt_t)whole);
    else mpq_div_2exp(scaled, scaled, (mp_bitcnt_t)-whole);
    if (mpq_cmp(scaled, high) > 0) mpq_set(scaled, high);
    if (mpq_cmp(scaled, low) < 0) mpq_set(scaled, low);
    mt_atom *out = mt_real(rounded(scaled));
    mpq_clears(logs, scaled, share, low, high, NULL);
    release(&s);
    return out;
}

/* By exact value. qsort moves each GMP struct whole, so every value keeps
   one owner. */
static int by_value(const void *a, const void *b) { return mpq_cmp(*(const mpq_t *)a, *(const mpq_t *)b); }

static void sort(sample *s) { qsort(s->v, s->n, sizeof *s->v, by_value); }

enum method { INCLUSIVE, EXCLUSIVE, METHODS };
static const char *const method_names[METHODS] = { "inclusive", "exclusive" };

static int method_of(const char *name)
{
    for (int i = 0; i < METHODS; i++)
        if (strcmp(method_names[i], name) == 0) return i;
    return -1;
}

/* Linear interpolation of a sorted sample at the exact probability p, placing
   observation i at i/(n-1) when inclusive and at (i+1)/(n+1) when
   exclusive, so an exclusive end extrapolates from the nearest pair [source:
   https://github.com/python/cpython/blob/ebf955df7a89ed0c7968f79faec1de49f61ed7cb/Lib/statistics.py#L1165-L1218]. */
static void interpolated(const sample *sorted, const mpq_t p, int method, mpq_t out)
{
    if (sorted->n == 1) {
        mpq_set(out, sorted->v[0]);
        return;
    }
    mpq_t position, delta, other;
    mpz_t j;
    mpq_inits(position, delta, other, NULL);
    mpz_init(j);
    mpq_set_ui(other, method == INCLUSIVE ? sorted->n - 1 : sorted->n + 1, 1);
    mpq_mul(position, p, other);
    if (method == EXCLUSIVE) {
        mpq_set_ui(other, 1, 1);
        mpq_sub(position, position, other);
    }
    mpz_fdiv_q(j, mpq_numref(position), mpq_denref(position));
    long at = mpz_sgn(j) < 0 ? 0 : mpz_cmp_ui(j, sorted->n - 2) > 0 ? (long)sorted->n - 2 : mpz_get_si(j);
    mpq_set_si(other, at, 1);
    mpq_sub(delta, position, other);
    mpq_mul(out, sorted->v[at + 1], delta);
    mpq_set_ui(other, 1, 1);
    mpq_sub(other, other, delta);
    mpq_mul(other, sorted->v[at], other);
    mpq_add(out, out, other);
    mpq_clears(position, delta, other, NULL);
    mpz_clear(j);
}

/* The quantile at probability p, a number in [0, 1]; NULL for no data, a p
   outside, or a method with no name here. */
static mt_atom *quantile(const mt_atom *data, const mt_atom *p, const char *method)
{
    int how = method_of(method);
    sample s;
    if (how < 0 || !read_sample(data, &s)) return NULL;
    mt_atom *out = NULL;
    mpq_t at, q;
    mpq_inits(at, q, NULL);
    exact(at, p);
    if (s.n > 0 && mpq_sgn(at) >= 0 && mpq_cmp_ui(at, 1, 1) <= 0) {
        sort(&s);
        interpolated(&s, at, how, q);
        out = result(s.floating, q);
    }
    mpq_clears(at, q, NULL);
    release(&s);
    return out;
}

static mt_atom *median(const mt_atom *data)
{
    mt_atom *half = mt_rational(1, 2), *out = quantile(data, half, "inclusive");
    mt_drop(half);
    return out;
}

/* The partitions - 1 cut points at i/partitions; NULL for no data, no
   partitions, or an unnamed method. */
static mt_atom *quantiles(const mt_atom *data, int64_t partitions, const char *method)
{
    int how = method_of(method);
    sample s;
    if (how < 0 || partitions <= 0 || !read_sample(data, &s)) return NULL;
    mt_atom *out = NULL;
    if (s.n > 0) {
        mt_atom **cuts = malloc((size_t)partitions * sizeof *cuts);
        require("room for the cuts", cuts != NULL);
        mpq_t at, q;
        mpq_inits(at, q, NULL);
        sort(&s);
        for (int64_t i = 1; i < partitions; i++) {
            mpq_set_si(at, i, (unsigned long)partitions);
            mpq_canonicalize(at);
            interpolated(&s, at, how, q);
            cuts[i - 1] = result(s.floating, q);
        }
        out = mt_exprv((size_t)partitions - 1, cuts);
        free(cuts);
        mpq_clears(at, q, NULL);
    }
    release(&s);
    return out;
}

/* The most frequent terms, each once, in the order they first appear, into
   *out, which the caller frees; terms group by identity, so 1 and 1.0 are
   two. 0 for no data. Time: n^2 identity comparisons for n terms. */
static size_t modes(const mt_atom *data, mt_atom ***out)
{
    size_t n = mt_len(data), groups = 0, most = 0, found = 0;
    size_t *first = malloc((n + 1) * sizeof *first), *count = malloc((n + 1) * sizeof *count);
    *out = malloc((n + 1) * sizeof **out);
    require("room for the groups", first && count && *out);
    for (size_t i = 0; i < n; i++) {
        size_t g = 0;
        while (g < groups && !mt_eq(mt_at(data, first[g]), mt_at(data, i))) g++;
        if (g == groups) first[groups] = i, count[groups++] = 0;
        if (++count[g] > most) most = count[g];
    }
    for (size_t g = 0; g < groups; g++)
        if (count[g] == most) (*out)[found++] = mt_keep(mt_at(data, first[g]));
    free(first);
    free(count);
    return found;
}

/* The exact covariance dividing by n - degrees; false when the samples
   differ in length or hold no more than `degrees` observations. */
static bool covariance_of(const sample *a, const sample *b, int64_t degrees, mpq_t out)
{
    if (a->n != b->n || degrees < 0 || a->n <= (uint64_t)degrees) return false;
    mpq_t xy, sa, sb, t;
    mpq_inits(xy, sa, sb, t, NULL);
    for (size_t i = 0; i < a->n; i++) {
        mpq_mul(t, a->v[i], b->v[i]);
        mpq_add(xy, xy, t);
    }
    sum_of(a, sa);
    sum_of(b, sb);
    mpq_set_ui(t, a->n, 1);
    mpq_mul(xy, xy, t);
    mpq_mul(sa, sa, sb);
    mpq_sub(out, xy, sa);
    mpq_set_ui(t, a->n * (a->n - (size_t)degrees), 1);
    mpq_div(out, out, t);
    mpq_clears(xy, sa, sb, t, NULL);
    return true;
}

/* Both samples read, or neither held. */
static bool read_pair(const mt_atom *left, const mt_atom *right, sample *a, sample *b)
{
    if (!read_sample(left, a)) return false;
    if (read_sample(right, b)) return true;
    release(a);
    return false;
}

static mt_atom *covariance(const mt_atom *left, const mt_atom *right, int64_t degrees)
{
    sample a, b;
    if (!read_pair(left, right, &a, &b)) return NULL;
    mt_atom *out = NULL;
    mpq_t c;
    mpq_init(c);
    if (covariance_of(&a, &b, degrees, c)) out = result(a.floating || b.floating, c);
    mpq_clear(c);
    release(&a);
    release(&b);
    return out;
}

static mt_atom *variance(const mt_atom *data, int64_t degrees) { return covariance(data, data, degrees); }

/* The root of the exact variance, rounded once. */
static mt_atom *stdev(const mt_atom *data, int64_t degrees)
{
    sample s;
    if (!read_sample(data, &s)) return NULL;
    mt_atom *out = NULL;
    mpq_t v;
    mpq_init(v);
    if (covariance_of(&s, &s, degrees, v)) out = mt_real(root(v));
    mpq_clear(v);
    release(&s);
    return out;
}

/* Pearson's r from exact population moments, its magnitude the root of
   xy^2 / (xx yy) rounded once; NULL for fewer than two pairs or a constant
   side. */
static mt_atom *correlation(const mt_atom *left, const mt_atom *right)
{
    sample a, b;
    if (!read_pair(left, right, &a, &b)) return NULL;
    mt_atom *out = NULL;
    mpq_t xx, yy, xy;
    mpq_inits(xx, yy, xy, NULL);
    if (a.n > 1 && covariance_of(&a, &a, 0, xx) && covariance_of(&b, &b, 0, yy) && covariance_of(&a, &b, 0, xy) &&
        mpq_sgn(xx) > 0 && mpq_sgn(yy) > 0) {
        mpq_mul(xx, xx, yy);
        mpq_mul(yy, xy, xy);
        mpq_div(yy, yy, xx);
        out = mt_real(mpq_sgn(xy) < 0 ? -root(yy) : root(yy));
    }
    mpq_clears(xx, yy, xy, NULL);
    release(&a);
    release(&b);
    return out;
}

/* One-based ranks, a tie taking the mean of the ranks it spans: (2 less +
   equal + 1) / 2, with 1 and 1.0 equal. */
static mt_atom *ranks(const mt_atom *data)
{
    sample s;
    if (!read_sample(data, &s)) return NULL;
    mt_atom **out = malloc((s.n + 1) * sizeof *out);
    require("room for the ranks", out != NULL);
    mpq_t rank;
    mpq_init(rank);
    for (size_t i = 0; i < s.n; i++) {
        unsigned long twice = 1;
        for (size_t j = 0; j < s.n; j++) {
            int order = mpq_cmp(s.v[j], s.v[i]);
            twice += order < 0 ? 2 : order == 0;
        }
        mpq_set_ui(rank, twice, 2);
        mpq_canonicalize(rank);
        out[i] = rational_of(rank);
    }
    mpq_clear(rank);
    mt_atom *ranked = mt_exprv(s.n, out);
    free(out);
    release(&s);
    return ranked;
}

/* Least squares through the exact moments: an affine fit needs two pairs
   and a nonconstant x, a fit through the origin one pair and a nonzero sum
   of squares. */
static mt_atom *regression(const mt_atom *left, const mt_atom *right, bool proportional)
{
    sample a, b;
    if (!read_pair(left, right, &a, &b)) return NULL;
    mt_atom *out = NULL;
    bool floating = a.floating || b.floating;
    mpq_t xx, xy, slope, intercept, t;
    mpq_inits(xx, xy, slope, intercept, t, NULL);
    bool fits = proportional ? a.n == b.n && a.n > 0 : a.n > 1 && covariance_of(&a, &a, 0, xx) && covariance_of(&a, &b, 0, xy);
    if (fits && proportional)
        for (size_t i = 0; i < a.n; i++) {
            mpq_mul(t, a.v[i], a.v[i]);
            mpq_add(xx, xx, t);
            mpq_mul(t, a.v[i], b.v[i]);
            mpq_add(xy, xy, t);
        }
    if (fits && mpq_sgn(xx) > 0) {
        mpq_div(slope, xy, xx);
        if (!proportional) {
            mean_of(&a, t);
            mpq_mul(t, t, slope);
            mean_of(&b, intercept);
            mpq_sub(intercept, intercept, t);
        }
        out = E("linear-fit", result(floating, slope), result(floating, intercept));
    }
    mpq_clears(xx, xy, slope, intercept, t, NULL);
    release(&a);
    release(&b);
    return out;
}

static mt_atom *known(mt_atom *value)
{
    require("the C statistic has a value", value != NULL);
    return value;
}

static bool computed(mt_atom *value)
{
    mt_drop(value);
    return value != NULL;
}

static mt_atom *verdict(bool holds) { return S(holds ? "fine" : "refused"); }
static mt_answers *guarded(metta *m, mt_atom *goal) { return mt_eval(m, E("if-error", E("catch", goal), "refused", "fine")); }

static mt_atom *value_of(metta *m, mt_atom *goal)
{
    mt_atom *v = mt_one(mt_eval(m, goal));
    require("a value", v != NULL);
    return v;
}

/* The function an equation of `head` spells, read back out of &self and
   evaluated: (|-> params body) over the head's variables named in params. */
static mt_atom *recipe(metta *m, mt_atom *head, const char *const *params, size_t count)
{
    mt_atom *lambda = NULL, **names = malloc((count + 1) * sizeof *names);
    require("room for the parameters", names != NULL);
    mt_rows (row, mt_match(m, E("=", head, V("body")))) {
        for (size_t i = 0; i < count; i++) names[i] = mt_keep(mt_bound(row, params[i]));
        mt_drop(lambda);
        lambda = E("|->", mt_exprv(count, names), mt_keep(mt_bound(row, "body")));
    }
    free(names);
    require("the equation is in &self", lambda != NULL);
    return value_of(m, lambda);
}

int main(void)
{
    metta *m = open_engine();
    require("import lib_statistics", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_statistics")))));

    /* Exact accumulation, rounded only where a float was observed. */
    mt_atom *none = mt_unit(), *d123 = E(1, 2, 3), *cancelling = E(1.0e308, 1, -1.0e308), *twice = E(1.0e308, 1.0e308),
            *d12 = E(1, 2), *d4060 = E(40, 60), *f4060 = E(40.0, 60), *d07 = E(0, 7), *d542436 = E(54, 24, 36);
    check_answers("the sum of nothing", mt_eval(m, E("stats-sum", mt_keep(none))), known(sum(none)));
    check_answers("a sum", mt_eval(m, E("stats-sum", mt_keep(d123))), known(sum(d123)));
    check_answers("exact until the one rounding", mt_eval(m, E("stats-sum", mt_keep(cancelling))), known(sum(cancelling)));
    check_answers("past binary64, infinity", mt_eval(m, E("stats-sum", mt_keep(twice))), known(sum(twice)));
    check_answers("a mean", mt_eval(m, E("stats-mean", mt_keep(d123))), known(mean(d123)));
    check_answers("an exact mean", mt_eval(m, E("stats-mean", mt_keep(d12))), known(mean(d12)));
    check_answers("a mean whose sum overflows", mt_eval(m, E("stats-mean", mt_keep(twice))), known(mean(twice)));
    check_answers("a third, rounded once", mt_eval(m, E("stats-mean", mt_keep(cancelling))), known(mean(cancelling)));
    check_answers("a harmonic mean", mt_eval(m, E("stats-harmonic-mean", mt_keep(d4060))), known(harmonic_mean(d4060)));
    check_answers("rounded for a float", mt_eval(m, E("stats-harmonic-mean", mt_keep(f4060))), known(harmonic_mean(f4060)));
    check_answers("zero with a zero", mt_eval(m, E("stats-harmonic-mean", mt_keep(d07))), known(harmonic_mean(d07)));
    check_answers("a geometric mean", mt_eval(m, E("stats-geometric-mean", mt_keep(d542436))), known(geometric_mean(d542436)));
    check_answers("zero again", mt_eval(m, E("stats-geometric-mean", mt_keep(d07))), known(geometric_mean(d07)));
    check_answers("no overflowing product", mt_eval(m, E("stats-geometric-mean", mt_keep(twice))), known(geometric_mean(twice)));
    mpz_t power;
    mpq_t inverse;
    mpz_init(power);
    mpq_init(inverse);
    mpz_ui_pow_ui(power, 2, 2000);
    mpq_set_z(inverse, power);
    mpq_inv(inverse, inverse);
    mt_atom *extremes = E(integer_of(power), rational_of(inverse));
    check_answers("exponents cancel exactly", mt_eval(m, E("stats-geometric-mean", mt_keep(extremes))), known(geometric_mean(extremes)));
    mpz_clear(power);
    mpq_clear(inverse);

    /* Quantiles by a named interpolation rule. */
    mt_atom *d914 = E(9, 1, 4), *f914 = E(9.0, 1, 4), *d010 = E(0, 10), *d048 = E(0, 4, 8), *seven = E(7);
    mt_atom *zero = mt_num(0), *one = mt_num(1), *quarter = mt_real(0.25);
    check_answers("a median", mt_eval(m, E("stats-median", mt_keep(d914))), known(median(d914)));
    check_answers("the mean of the middle two", mt_eval(m, E("stats-median", mt_keep(d12))), known(median(d12)));
    check_answers("a float median", mt_eval(m, E("stats-median", mt_keep(f914))), known(median(f914)));
    check_answers("inclusive at 0", mt_eval(m, E("stats-quantile", mt_keep(d010), mt_keep(zero), "inclusive")),
                  known(quantile(d010, zero, "inclusive")));
    check_answers("inclusive at 1", mt_eval(m, E("stats-quantile", mt_keep(d010), mt_keep(one), "inclusive")),
                  known(quantile(d010, one, "inclusive")));
    check_answers("inclusive at a quarter", mt_eval(m, E("stats-quantile", mt_keep(d010), mt_keep(quarter), "inclusive")),
                  known(quantile(d010, quarter, "inclusive")));
    check_answers("exclusive extrapolates below", mt_eval(m, E("stats-quantile", mt_keep(d010), mt_keep(zero), "exclusive")),
                  known(quantile(d010, zero, "exclusive")));
    check_answers("and above", mt_eval(m, E("stats-quantile", mt_keep(d010), mt_keep(one), "exclusive")),
                  known(quantile(d010, one, "exclusive")));
    check_answers("inclusive quartiles", mt_eval(m, E("stats-quantiles", mt_keep(d048), 4, "inclusive")), known(quantiles(d048, 4, "inclusive")));
    check_answers("exclusive quartiles", mt_eval(m, E("stats-quantiles", mt_keep(d048), 4, "exclusive")), known(quantiles(d048, 4, "exclusive")));
    check_answers("a singleton's quartiles", mt_eval(m, E("stats-quantiles", mt_keep(seven), 4, "exclusive")), known(quantiles(seven, 4, "exclusive")));
    check_answers("one partition, no cuts", mt_eval(m, E("stats-quantiles", mt_keep(seven), 1, "inclusive")), known(quantiles(seven, 1, "inclusive")));

    /* Modes: every tie is an answer, in first-occurrence order. */
    mt_atom *letters = E(T("b"), T("a"), T("b"), T("a"), T("c")), *four = E(T("b"), T("a"), T("b"), T("a")), *kinds = E(1, 1.0),
            *terms = E(E("+", 1, 2), E("+", 1, 2), 9), **found;
    size_t n = modes(letters, &found);
    check_answers_("the tied modes", mt_eval(m, E("stats-mode", mt_keep(letters))), n, found);
    free(found);
    n = modes(four, &found);
    check_atom("the first mode", mt_first(mt_eval(m, E("stats-mode", mt_keep(four)))), mt_keep(found[0]));
    for (size_t i = 0; i < n; i++) mt_drop(found[i]);
    free(found);
    n = modes(kinds, &found);
    check_answers_("1 and 1.0 are two terms", mt_eval(m, E("stats-mode", mt_keep(kinds))), n, found);
    free(found);
    n = modes(terms, &found);
    check_answers_("a mode stays data", mt_eval(m, E("stats-mode", mt_keep(terms))), n, found);
    free(found);

    /* One degrees-of-freedom argument covers every divisor. */
    mt_atom *billions = E(1000000000.0, 1000000001.0, 1000000002.0), *d13 = E(1, 3), *huge = E(-1.0e308, 1.0e308),
            *tiny = E(-1.0e-300, 1.0e-300), *d135 = E(1, 3, 5), *flipped = E(1.0e308, -1.0e308);
    check_answers("a sample variance", mt_eval(m, E("stats-variance", mt_keep(d123), 1)), known(variance(d123, 1)));
    check_answers("a population variance", mt_eval(m, E("stats-variance", mt_keep(d123), 0)), known(variance(d123, 0)));
    check_answers("two degrees", mt_eval(m, E("stats-variance", mt_keep(d123), 2)), known(variance(d123, 2)));
    check_answers("no cancellation", mt_eval(m, E("stats-variance", mt_keep(billions), 1)), known(variance(billions, 1)));
    check_answers("a singleton's spread", mt_eval(m, E("stats-variance", mt_keep(seven), 0)), known(variance(seven, 0)));
    check_answers("a deviation", mt_eval(m, E("stats-stdev", mt_keep(d123), 1)), known(stdev(d123, 1)));
    check_answers("a population deviation", mt_eval(m, E("stats-stdev", mt_keep(d13), 0)), known(stdev(d13, 0)));
    check_answers("a variance past binary64", mt_eval(m, E("stats-variance", mt_keep(huge), 0)), known(variance(huge, 0)));
    check_answers("whose root is not", mt_eval(m, E("stats-stdev", mt_keep(huge), 0)), known(stdev(huge, 0)));
    check_answers("a variance below binary64", mt_eval(m, E("stats-variance", mt_keep(tiny), 0)), known(variance(tiny, 0)));
    check_answers("whose root is not either", mt_eval(m, E("stats-stdev", mt_keep(tiny), 0)), known(stdev(tiny, 0)));
    check_answers("a covariance", mt_eval(m, E("stats-covariance", mt_keep(d123), mt_keep(d135), 1)), known(covariance(d123, d135, 1)));
    check_answers("a population covariance", mt_eval(m, E("stats-covariance", mt_keep(d123), mt_keep(d135), 0)),
                  known(covariance(d123, d135, 0)));
    check_answers("perfectly correlated", mt_eval(m, E("stats-correlation", mt_keep(huge), mt_keep(huge))), known(correlation(huge, huge)));
    check_answers("perfectly anticorrelated", mt_eval(m, E("stats-correlation", mt_keep(huge), mt_keep(flipped))),
                  known(correlation(huge, flipped)));
    mt_atom *d302010 = E(30, 10, 20), *tied = E(1, 1.0, 2), *d149 = E(1, 4, 9);
    check_answers("no ranks", mt_eval(m, E("stats-ranks", mt_keep(none))), known(ranks(none)));
    check_answers("ranks", mt_eval(m, E("stats-ranks", mt_keep(d302010))), known(ranks(d302010)));
    check_answers("ties share their mean rank", mt_eval(m, E("stats-ranks", mt_keep(tied))), known(ranks(tied)));
    mt_atom *ranked_149 = value_of(m, E("stats-ranks", mt_keep(d149))), *ranked_123 = value_of(m, E("stats-ranks", mt_keep(d123)));
    check_answers("Spearman's rank correlation", mt_eval(m, E("stats-correlation", mt_keep(ranked_149), mt_keep(ranked_123))),
                  known(correlation(ranked_149, ranked_123)));
    mt_atom *d012 = E(0, 1, 2), *f012 = E(0.0, 1, 2), *d159 = E(1, 5, 9), *two = E(2), *six = E(6), *level = E(7, 7, 7);
    check_answers("an affine fit", mt_eval(m, E("stats-regression", mt_keep(d012), mt_keep(d159), B(false))), known(regression(d012, d159, false)));
    check_answers("rounded for a float", mt_eval(m, E("stats-regression", mt_keep(f012), mt_keep(d159), B(false))),
                  known(regression(f012, d159, false)));
    check_answers("through the origin", mt_eval(m, E("stats-regression", mt_keep(two), mt_keep(six), B(true))), known(regression(two, six, true)));
    check_answers("a level line", mt_eval(m, E("stats-regression", mt_keep(d012), mt_keep(level), B(false))), known(regression(d012, level, false)));

    /* The recipe, read back out of the space and applied; a match can bind
       the degrees before the body becomes a function. */
    static const char *const both[] = { "data", "degrees" }, *const data_only[] = { "data" };
    mt_atom *spread = recipe(m, E("stats-variance", V("data"), V("degrees")), both, 2);
    check_answers("the variance's own recipe", mt_eval(m, E(mt_keep(spread), mt_keep(d123), 1)), known(variance(d123, 1)));
    mt_atom *population = recipe(m, E("stats-variance", V("data"), 0), data_only, 1);
    check_answers("specialized to a population", mt_eval(m, E(mt_keep(population), mt_keep(d123))), known(variance(d123, 0)));

    /* Refusals look at the whole sample, even where a zero or a singleton
       could have answered early. */
    mt_atom *bad = E(1, T("bad")), *negative = E(0, -1), *d11 = E(1, 1), *d23 = E(2, 3), *just1 = E(1), *just2 = E(2), *zero1 = E(0),
            *three = E(3), *infinite = E(mt_real(INFINITY)), *two_prob = mt_num(2), *half = mt_real(0.5);
    check_answers("a mean of nothing", guarded(m, E("stats-mean", mt_keep(none))), verdict(computed(mean(none))));
    check_answers("text is no observation", guarded(m, E("stats-sum", mt_keep(bad))), verdict(computed(sum(bad))));
    check_answers("a geometric mean of nothing", guarded(m, E("stats-geometric-mean", mt_keep(none))),
                  verdict(computed(geometric_mean(none))));
    check_answers("or of a negative", guarded(m, E("stats-geometric-mean", mt_keep(negative))),
                  verdict(computed(geometric_mean(negative))));
    check_answers("a harmonic mean of nothing", guarded(m, E("stats-harmonic-mean", mt_keep(none))),
                  verdict(computed(harmonic_mean(none))));
    check_answers("or of a negative", guarded(m, E("stats-harmonic-mean", mt_keep(negative))),
                  verdict(computed(harmonic_mean(negative))));
    check_answers("a median of nothing", guarded(m, E("stats-median", mt_keep(none))), verdict(computed(median(none))));
    check_answers("a probability past 1", guarded(m, E("stats-quantile", mt_keep(just1), mt_keep(two_prob), "inclusive")),
                  verdict(computed(quantile(just1, two_prob, "inclusive"))));
    check_answers("an unnamed method", guarded(m, E("stats-quantile", mt_keep(just1), mt_keep(half), "missing")),
                  verdict(computed(quantile(just1, half, "missing"))));
    check_answers("no partitions", guarded(m, E("stats-quantiles", mt_keep(just1), 0, "inclusive")),
                  verdict(computed(quantiles(just1, 0, "inclusive"))));
    check_answers("partitions by an unnamed method", guarded(m, E("stats-quantiles", mt_keep(just1), 1, "missing")),
                  verdict(computed(quantiles(just1, 1, "missing"))));
    n = modes(none, &found);
    free(found);
    check_answers("a mode of nothing", guarded(m, E("stats-mode", mt_keep(none))), verdict(n > 0));
    check_answers("one observation, one degree", guarded(m, E("stats-variance", mt_keep(just1), 1)),
                  verdict(computed(variance(just1, 1))));
    check_answers("negative degrees", guarded(m, E("stats-variance", mt_keep(d12), -1)), verdict(computed(variance(d12, -1))));
    check_answers("a deviation of nothing", guarded(m, E("stats-stdev", mt_keep(none), 0)), verdict(computed(stdev(none, 0))));
    check_answers("unpaired lengths", guarded(m, E("stats-covariance", mt_keep(d12), mt_keep(just1), 0)),
                  verdict(computed(covariance(d12, just1, 0))));
    check_answers("a constant side", guarded(m, E("stats-correlation", mt_keep(d11), mt_keep(d23))),
                  verdict(computed(correlation(d11, d23))));
    check_answers("a single pair", guarded(m, E("stats-correlation", mt_keep(just1), mt_keep(just2))),
                  verdict(computed(correlation(just1, just2))));
    check_answers("a constant x", guarded(m, E("stats-regression", mt_keep(d11), mt_keep(d23), B(false))),
                  verdict(computed(regression(d11, d23, false))));
    check_answers("no squares through the origin", guarded(m, E("stats-regression", mt_keep(zero1), mt_keep(just2), B(true))),
                  verdict(computed(regression(zero1, just2, true))));
    check_answers("unpaired regression", guarded(m, E("stats-regression", mt_keep(d12), mt_keep(three), B(false))),
                  verdict(computed(regression(d12, three, false))));
    check_answers("infinity is no observation", guarded(m, E("stats-mean", mt_keep(infinite))), verdict(computed(mean(infinite))));

    mt_atom *held[] = { none, d123, cancelling, twice, d12, d4060, f4060, d07, d542436, extremes, d914, f914, d010, d048, seven,
                        zero, one, quarter, letters, four, kinds, terms, billions, d13, huge, tiny, d135, flipped, d302010, tied,
                        d149, ranked_149, ranked_123, d012, f012, d159, two, six, level, spread, population, bad, negative, d11,
                        d23, just1, just2, zero1, three, infinite, two_prob, half };
    for (size_t i = 0; i < sizeof held / sizeof *held; i++) mt_drop(held[i]);
    return done(m);
}
