/* Purpose: C's model of lib_measure's weighted superpositions: a pair is a
 *   weight and the value it measures, a superposition an array of pairs in
 *   order, and each operation the C function lib_measure's equation
 *   describes [source: lib/lib_measure/lib.metta, ws-total to ws-flip;
 *   commit=8d651070dedaa190e25cc388c029172a63e967be]:
 *   - the total folds the weights from 0.0 in order;
 *   - of two pairs the heavier is kept and the first on a tie, so the best
 *     pair and the peak are left-biased folds from the first pair;
 *   - collapsing merges each pair into the first earlier one with the same
 *     value, summing the weights, and appends it where there is none;
 *   - normalizing refuses an empty, negative, non-finite or weightless
 *     superposition, drops the pairs without weight, collapses the rest and
 *     divides by their total unless it is already one, first dividing by
 *     the peak where the total overflows;
 *   - softmax divides by the temperature, subtracts the peak, exponentiates
 *     and normalizes;
 *   - ranking sorts the pairs in the standard order of terms, which reads
 *     the weight first, and reverses;
 *   - the sampling walk subtracts weights until the budget is below one,
 *     and the last pair takes whatever is left.
 *   Beside the model are the builders a twin states its superpositions
 *   with.
 * Assumes: the includer includes common.h first; every weight is a float,
 *   as every weight these programs write is, since C keeps a weight as a
 *   double and would answer 2.0 for an integer 2.
 * Guarantees: each function answers what lib_measure answers for the same
 *   pairs, and refuses exactly where it answers an Error [tested: make
 *   twins; commit=WORKTREE].
 * Owns resources: a superposition owns its values and its array, which
 *   ws_free() releases.
 */
#ifndef CH22_MEASURE_H
#define CH22_MEASURE_H
#include <math.h>

typedef struct weighted {
    double weight;
    mt_atom *value;
} weighted;

typedef struct ws {
    weighted *pairs;
    size_t n;
} ws;

/* Append (WEIGHT VALUE), VALUE TAKEN. */
static inline void ws_push(ws *s, double weight, mt_atom *value)
{
    weighted *grown = realloc(s->pairs, (s->n + 1) * sizeof *grown);
    require("room", grown != NULL);
    s->pairs = grown;
    s->pairs[s->n++] = (weighted){ weight, value };
}

static inline void ws_free(ws *s)
{
    for (size_t i = 0; i < s->n; i++) mt_drop(s->pairs[i].value);
    free(s->pairs);
    *s = (ws){ NULL, 0 };
}

/* Each pair's weight passed through F, the values kept. */
static inline ws ws_map(const ws *s, double (*f)(double weight, double by), double by)
{
    ws out = { NULL, 0 };
    for (size_t i = 0; i < s->n; i++) ws_push(&out, f(s->pairs[i].weight, by), mt_keep(s->pairs[i].value));
    return out;
}
static inline double ws_divided(double weight, double by) { return weight / by; }
static inline double ws_shifted_exp(double weight, double by) { return exp(weight - by); }

static inline mt_atom *pair_atom(const weighted *p) { return mt_expr(p->weight, mt_keep(p->value)); }

/* ((weight value) ...) */
static inline mt_atom *ws_atom(const ws *s)
{
    mt_atom **pairs = malloc((s->n ? s->n : 1) * sizeof *pairs);
    require("room", pairs != NULL);
    for (size_t i = 0; i < s->n; i++) pairs[i] = pair_atom(&s->pairs[i]);
    mt_atom *atom = mt_exprv(s->n, pairs);
    free(pairs);
    return atom;
}

/* A superposition of rows of C's own, each value a symbol:
   WS({ 0.5, "a" }, { 0.5, "b" }) is ((0.5 a) (0.5 b)). */
typedef struct ws_row {
    double weight;
    const char *value;
} ws_row;

static inline ws ws_of(const ws_row *rows, size_t n)
{
    ws s = { NULL, 0 };
    for (size_t i = 0; i < n; i++) ws_push(&s, rows[i].weight, mt_sym(rows[i].value));
    return s;
}
#define WS(...) ws_of((const ws_row[]){ __VA_ARGS__ }, sizeof((const ws_row[]){ __VA_ARGS__ }) / sizeof(ws_row))

/* The atom of a superposition C built, which is released. */
static inline mt_atom *ws_atom_of(ws s)
{
    mt_atom *atom = ws_atom(&s);
    ws_free(&s);
    return atom;
}

static inline double ws_total(const ws *s)
{
    double total = 0.0;
    for (size_t i = 0; i < s->n; i++) total = total + s->pairs[i].weight;
    return total;
}

static inline const weighted *ws_pickmax(const weighted *a, const weighted *b) { return a->weight >= b->weight ? a : b; }

/* The fold of ws_pickmax from the first pair; NULL for no pairs. */
static inline const weighted *ws_best_pair(const ws *s)
{
    const weighted *best = s->n ? &s->pairs[0] : NULL;
    for (size_t i = 1; i < s->n; i++) best = ws_pickmax(best, &s->pairs[i]);
    return best;
}

/* merge-into: ACC with P summed into its first pair of P's value, or P
   appended. P's value is kept. */
static inline void ws_merge_into(ws *acc, const weighted *p)
{
    for (size_t i = 0; i < acc->n; i++)
        if (mt_eq(acc->pairs[i].value, p->value)) {
            acc->pairs[i].weight = acc->pairs[i].weight + p->weight;
            return;
        }
    ws_push(acc, p->weight, mt_keep(p->value));
}

static inline ws ws_collapse(const ws *s)
{
    ws acc = { NULL, 0 };
    for (size_t i = 0; i < s->n; i++) ws_merge_into(&acc, &s->pairs[i]);
    return acc;
}

/* The distribution, or false where lib_measure answers an Error. */
static inline bool ws_normalize(const ws *s, ws *out)
{
    if (s->n == 0) return false;
    for (size_t i = 0; i < s->n; i++)
        if (s->pairs[i].weight < 0.0) return false;
    for (size_t i = 0; i < s->n; i++)
        if (isnan(s->pairs[i].weight) || isinf(s->pairs[i].weight)) return false;
    ws positive = { NULL, 0 };
    for (size_t i = 0; i < s->n; i++)
        if (s->pairs[i].weight > 0.0) ws_push(&positive, s->pairs[i].weight, mt_keep(s->pairs[i].value));
    bool ok = positive.n > 0;
    if (ok) {
        double total = ws_total(&positive);
        if (isinf(total)) {
            ws scaled = ws_map(&positive, ws_divided, ws_best_pair(&positive)->weight);
            ok = ws_normalize(&scaled, out);
            ws_free(&scaled);
        } else {
            ws collapsed = ws_collapse(&positive);
            if (total == 1.0)
                *out = collapsed;
            else
                *out = ws_map(&collapsed, ws_divided, total), ws_free(&collapsed);
        }
    }
    ws_free(&positive);
    return ok;
}

/* Softmax at TEMPERATURE, or false where lib_measure answers an Error. */
static inline bool ws_softmax(const ws *s, double temperature, ws *out)
{
    if (fabs(temperature) <= 0.0) return false;
    if (s->n == 0) return *out = (ws){ NULL, 0 }, true;
    ws scaled = ws_map(s, ws_divided, temperature);
    ws exps = ws_map(&scaled, ws_shifted_exp, ws_best_pair(&scaled)->weight);
    bool ok = ws_normalize(&exps, out);
    ws_free(&scaled), ws_free(&exps);
    return ok;
}

/* The pairs best-first: the standard order of the pair atoms, reversed. */
static inline ws ws_ranked(const ws *s)
{
    mt_atom **pairs = malloc((s->n ? s->n : 1) * sizeof *pairs);
    require("room", pairs != NULL);
    for (size_t i = 0; i < s->n; i++) pairs[i] = pair_atom(&s->pairs[i]);
    qsort(pairs, s->n, sizeof *pairs, mt_order);
    ws ranked = { NULL, 0 };
    for (size_t i = s->n; i-- > 0;) {
        ws_push(&ranked, mt_float(mt_at(pairs[i], 0)), mt_keep(mt_at(pairs[i], 1)));
        mt_drop(pairs[i]);
    }
    free(pairs);
    return ranked;
}

/* The first K pairs, none for K of zero or less. */
static inline ws ws_take(const ws *s, int64_t k)
{
    ws taken = { NULL, 0 };
    for (size_t i = 0; i < s->n && (int64_t)i < k; i++) ws_push(&taken, s->pairs[i].weight, mt_keep(s->pairs[i].value));
    return taken;
}

static inline ws ws_top(const ws *s, int64_t k)
{
    ws ranked = ws_ranked(s), top = ws_take(&ranked, k);
    ws_free(&ranked);
    return top;
}

/* The value the inverse-transform walk lands on for BUDGET; NULL for no
   pairs, which the library's decons-atom cannot split either. */
static inline const mt_atom *ws_sample_walk(const ws *s, double budget)
{
    for (size_t i = 0; i < s->n; i++) {
        if (i + 1 == s->n || budget < s->pairs[i].weight) return s->pairs[i].value;
        budget = budget - s->pairs[i].weight;
    }
    return NULL;
}

/* The expected value of a numeric superposition. */
static inline double ws_expect(const ws *s)
{
    double sum = 0.0;
    for (size_t i = 0; i < s->n; i++) sum = sum + s->pairs[i].weight * mt_float(s->pairs[i].value);
    return sum;
}

static inline ws ws_filter(const ws *s, double least)
{
    ws kept = { NULL, 0 };
    for (size_t i = 0; i < s->n; i++)
        if (s->pairs[i].weight >= least) ws_push(&kept, s->pairs[i].weight, mt_keep(s->pairs[i].value));
    return kept;
}
#endif
