/* Purpose: lib_combinatorics, held against the textbook C algorithm for each
 *   choice: k-combinations in lexicographic index order, permutations by
 *   choosing each position's item in turn from those left, the powerset as a
 *   bitmask counted down from all ones with item i at bit i (so the whole set
 *   comes first and the empty one last), and products as an odometer whose
 *   last wheel turns fastest. The counts are C arithmetic, exact past 64 bits
 *   through a small decimal bignum, and each refusal is C's own precondition
 *   failing: a zero stride, a negative or fractional count, a set that is not
 *   a list, a float range that stops moving.
 * Guarantees: all sixty-three claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

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
enum { MOST = 64, LIMBS = 8 };

/* Answers a generator builds, each owned. */
typedef struct answers { size_t n; mt_atom *item[MOST]; } answers;

static void emit(answers *out, mt_atom *const *chosen, size_t k)
{
    mt_atom *kids[MOST];
    require("room for the answer", out->n < MOST);
    for (size_t i = 0; i < k; i++) kids[i] = mt_keep(chosen[i]);
    out->item[out->n++] = mt_exprv(k, kids);
}

static mt_atom *collected(answers a) { return mt_exprv(a.n, a.item); }

/* How many answers a generator built, which are then released. */
static int64_t how_many(answers a)
{
    for (size_t i = 0; i < a.n; i++) mt_drop(a.item[i]);
    return (int64_t)a.n;
}

/* Whether two counts agree, both released. */
static bool same(mt_atom *a, mt_atom *b)
{
    bool equal = mt_eq(a, b);
    mt_drop(a);
    mt_drop(b);
    return equal;
}

/* k of n items, unordered and without repetition, in index order. */
static void choose(mt_atom *const *items, size_t n, size_t k, size_t start, mt_atom **chosen, size_t depth, answers *out)
{
    if (depth == k) {
        emit(out, chosen, k);
        return;
    }
    for (size_t i = start; i + (k - depth) <= n; i++) {
        chosen[depth] = items[i];
        choose(items, n, k, i + 1, chosen, depth + 1, out);
    }
}

static answers combinations(mt_atom *const *items, size_t n, size_t k)
{
    answers out = { 0 };
    mt_atom *chosen[MOST];
    if (k <= n) choose(items, n, k, 0, chosen, 0, &out);
    return out;
}

static answers sorted(answers a)
{
    qsort(a.item, a.n, sizeof *a.item, mt_order);
    return a;
}

/* Every ordering: each item left takes the next position in turn. */
static void arrange(mt_atom **left, size_t n, mt_atom **placed, size_t depth, answers *out)
{
    if (n == 0) {
        emit(out, placed, depth);
        return;
    }
    for (size_t i = 0; i < n; i++) {
        mt_atom *rest[MOST];
        size_t r = 0;
        for (size_t j = 0; j < n; j++)
            if (j != i) rest[r++] = left[j];
        placed[depth] = left[i];
        arrange(rest, r, placed, depth + 1, out);
    }
}

static answers permutations(mt_atom *const *items, size_t n)
{
    answers out = { 0 };
    mt_atom *left[MOST], *placed[MOST];
    memcpy(left, items, n * sizeof *items);
    arrange(left, n, placed, 0, &out);
    return out;
}

static answers subsets(mt_atom *const *items, size_t n)
{
    answers out = { 0 };
    mt_atom *chosen[MOST] = { 0 };
    for (unsigned long mask = (1ul << n); mask-- > 0;) {
        size_t k = 0;
        for (size_t i = 0; i < n; i++)
            if (mask >> i & 1) chosen[k++] = items[i];
        emit(&out, chosen, k);
    }
    return out;
}

/* One item from each of m sets, the last wheel turning fastest. */
static answers product(mt_atom *const *const *sets, const size_t *sizes, size_t m)
{
    answers out = { 0 };
    size_t wheel[MOST] = { 0 };
    mt_atom *chosen[MOST] = { 0 };
    for (size_t i = 0; i < m; i++)
        if (sizes[i] == 0) return out;
    for (;;) {
        for (size_t i = 0; i < m; i++) chosen[i] = sets[i][wheel[i]];
        emit(&out, chosen, m);
        size_t i = m;
        while (i > 0 && ++wheel[i - 1] == sizes[i - 1]) wheel[--i] = 0;
        if (i == 0) return out;
    }
}

static answers power(mt_atom *const *set, size_t size, size_t k)
{
    mt_atom *const *sets[MOST];
    size_t sizes[MOST];
    for (size_t i = 0; i < k; i++) {
        sets[i] = set;
        sizes[i] = size;
    }
    return product(sets, sizes, k);
}

static answers numbers(int64_t from, int64_t to)
{
    answers out = { 0 };
    for (int64_t x = from; x < to; x++) out.item[out.n++] = mt_num(x);
    return out;
}

/* range-step's walk: towards the end by the stride, integers when every
   bound is one and floats otherwise. */
static answers stepped(double from, double to, double step, bool floats)
{
    answers out = { 0 };
    for (double x = from; step > 0 ? x < to : x > to; x += step)
        out.item[out.n++] = floats ? mt_real(x) : mt_num((int64_t)x);
    return out;
}

/* A float range that stops moving: its first value, then a refusal where the
   next would be, since start + step == start never arrives. */
static mt_atom *stalled(double from, double to, double step)
{
    mt_atom *said[MOST];
    size_t n = 0;
    for (double x = from; x < to && n < MOST; x += step) {
        said[n++] = mt_sym("fine");
        if (x + step == x) {
            said[n++] = mt_sym("refused");
            break;
        }
    }
    return mt_exprv(n, said);
}

/* A nonnegative integer as base-10^9 limbs, least significant first: the
   exact counts outgrow int64_t, 100 choose 50 among them. */
typedef struct big { uint32_t limb[LIMBS]; size_t n; } big;

static big big_of(uint32_t x) { return (big){ { x }, 1 }; }

static void big_mul(big *b, uint32_t factor)
{
    uint64_t carry = 0;
    for (size_t i = 0; i < b->n; i++) {
        uint64_t v = (uint64_t)b->limb[i] * factor + carry;
        b->limb[i] = (uint32_t)(v % 1000000000u);
        carry = v / 1000000000u;
    }
    while (carry) {
        require("room for the count", b->n < LIMBS);
        b->limb[b->n++] = (uint32_t)(carry % 1000000000u);
        carry /= 1000000000u;
    }
}

/* Exact division, which each step of the multiplicative binomial is. */
static void big_div(big *b, uint32_t divisor)
{
    uint64_t rest = 0;
    for (size_t i = b->n; i-- > 0;) {
        uint64_t v = rest * 1000000000u + b->limb[i];
        b->limb[i] = (uint32_t)(v / divisor);
        rest = v % divisor;
    }
    require("the division is exact", rest == 0);
    while (b->n > 1 && b->limb[b->n - 1] == 0) b->n--;
}

static mt_atom *big_atom(const big *b)
{
    char text[LIMBS * 9 + 1];
    size_t used = (size_t)snprintf(text, sizeof text, "%u", b->limb[b->n - 1]);
    for (size_t i = b->n - 1; i-- > 0;) used += (size_t)snprintf(text + used, sizeof text - used, "%09u", b->limb[i]);
    return b->n <= 2 ? mt_num((int64_t)strtoll(text, NULL, 10)) : mt_bigint(text);
}

static mt_atom *factorial(uint32_t n)
{
    big b = big_of(1);
    for (uint32_t i = 2; i <= n; i++) big_mul(&b, i);
    return big_atom(&b);
}

static mt_atom *binomial(uint32_t n, uint32_t k)
{
    if (k > n) return mt_num(0);
    big b = big_of(1);
    for (uint32_t i = 1; i <= k; i++) {
        big_mul(&b, n - k + i);
        big_div(&b, i);
    }
    return big_atom(&b);
}

static mt_atom *falling(uint32_t n, uint32_t k)
{
    big b = big_of(1);
    for (uint32_t i = n - k + 1; i <= n; i++) big_mul(&b, i);
    return big_atom(&b);
}

/* What if-error answers for a goal: refused when C's precondition for it
   fails, fine when it holds. A count is an integer at least zero, a stride
   one that moves, and each set of a product a list. */
static mt_atom *verdict(bool holds) { return mt_sym(holds ? "fine" : "refused"); }
static bool a_count(const mt_atom *n) { return mt_kind_of(n) == MT_INT && mt_int(n) >= 0; }
static bool a_set(const mt_atom *s) { return mt_kind_of(s) == MT_EXPR; }

static mt_atom *list_of(mt_atom *const *items, size_t n)
{
    mt_atom *kids[MOST];
    for (size_t i = 0; i < n; i++) kids[i] = mt_keep(items[i]);
    return mt_exprv(n, kids);
}

static mt_atom *guarded(mt_atom *goal)
{
    return E("if-error", E("catch", goal), "refused", "fine");
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_combinatorics",
            mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_combinatorics")))));

    mt_atom *abcd[] = { mt_sym("a"), mt_sym("b"), mt_sym("c"), mt_sym("d") };
    mt_atom *ab[] = { abcd[0], abcd[1] }, *abc[] = { abcd[0], abcd[1], abcd[2] };
#define L(items, n) list_of((items), (n))

    /* Unordered pairs and k-subsets. */
    assert(answers_are(mt_eval(m, E("sort-atom", E("collapse", E("choose2", L(abc, 3))))), E(collected(sorted(combinations(abc, 3, 2)))))
           && "choose2 answers every unordered pair");
    assert(answers_are(mt_eval(m, E("collapse", E("choose2", L(abc, 1)))), E(collected(combinations(abc, 1, 2))))
           && "a single item has no pair");
    assert(answers_are(mt_eval(m, E("collapse", E("choose2", mt_unit()))), E(collected(combinations(abc, 0, 2)))) && "nor has nothing");
    assert(answers_are(mt_eval(m, E("choose2l", L(abc, 3))), E(collected(combinations(abc, 3, 2)))) && "choose2l collects them");
    assert(answers_are(mt_eval(m, E("choose2l", L(abc, 1))), E(collected(combinations(abc, 1, 2)))) && "and of one item, none");
    assert(answers_are(mt_eval(m, E("chooseKl", L(abc, 3), 2)), E(collected(combinations(abc, 3, 2)))) && "chooseKl, k of them");
    assert(answers_are(mt_eval(m, E("chooseKl", L(abcd, 4), 3)), E(collected(combinations(abcd, 4, 3)))) && "three of four");
    mt_atom *two = collected(combinations(abc, 3, 2)), *pairs = collected(combinations(abc, 3, 2));
    assert(answers_are(mt_eval(m, E("==", E("chooseKl", L(abc, 3), 2), E("choose2l", L(abc, 3)))), E(B(mt_eq(two, pairs))))
           && "chooseKl at two is choose2l");
    mt_drop(two);
    mt_drop(pairs);
    assert(answers_are(mt_eval(m, E("chooseKl", L(abc, 3), 0)), E(collected(combinations(abc, 3, 0))))
           && "choosing none is the one empty choice");
    assert(answers_are(mt_eval(m, E("chooseKl", mt_unit(), 2)), E(collected(combinations(abc, 0, 2))))
           && "choosing some of nothing is none");
    assert(answers_are(mt_eval(m, E("chooseKl", mt_unit(), 0)), E(collected(combinations(abc, 0, 0))))
           && "and none of nothing, the empty one");
    assert(answers_are(mt_eval(m, E("sort-atom", E("collapse", E("chooseK", L(abc, 3), 2)))), E(collected(sorted(combinations(abc, 3, 2)))))
           && "chooseK streams them");
    assert(answers_are(mt_eval(m, E("collapse", E("chooseK", L(abc, 3), 0))), E(collected(combinations(abc, 3, 0))))
           && "and streams the empty choice");

    /* Prefixes. */
    static const size_t takes[][2] = { { 2, 3 }, { 0, 3 }, { 5, 3 }, { 2, 0 } };
    for (size_t i = 0; i < COUNT(takes); i++) {
        size_t k = takes[i][0], n = takes[i][1];
        assert(answers_are(mt_eval(m, E("takeK", (int64_t)k, L(abc, n))), E(L(abc, k < n ? k : n))) && "takeK is the prefix");
    }

    /* Ranges. */
    assert(answers_are(mt_eval(m, E("collapse", E("range", 0, 4))), E(collected(numbers(0, 4)))) && "range counts up, the end excluded");
    static const double strides[][3] = { { 0, 10, 3 }, { 5, 0, -2 }, { 0, 0, 1 }, { 0, 5, -1 } };
    for (size_t i = 0; i < COUNT(strides); i++) {
        const double *s = strides[i];
        assert(answers_are(mt_eval(m, E("collapse", E("range-step", (int64_t)s[0], (int64_t)s[1], (int64_t)s[2]))), E(collected(stepped(s[0], s[1], s[2], false))))
               && "range-step walks by its stride");
    }
    const int64_t still = 0;
    assert(answers_are(mt_eval(m, guarded(E("collapse", E("range-step", 0, 5, still)))), E(verdict(still != 0)))
           && "a zero stride never arrives");

    /* Orderings, subsets and products. */
    assert(answers_are(mt_eval(m, E("collapse", E("permutations", L(abc, 3)))), E(collected(permutations(abc, 3)))) && "every ordering");
    assert(answers_are(mt_eval(m, E("collapse", E("permutations", mt_unit()))), E(collected(permutations(abc, 0)))) && "of nothing, one");
    mt_atom *aa[] = { abcd[0], abcd[0] };
    assert(answers_are(mt_eval(m, E("size-atom", E("collapse", E("permutations", L(aa, 2))))), E(how_many(permutations(aa, 2))))
           && "positions, not values");
    assert(answers_are(mt_eval(m, E("collapse", E("subsets", L(ab, 2)))), E(collected(subsets(ab, 2)))) && "the powerset, whole first");
    assert(answers_are(mt_eval(m, E("collapse", E("subsets", mt_unit()))), E(collected(subsets(ab, 0)))) && "of nothing, the empty set");
    assert(answers_are(mt_eval(m, E("size-atom", E("collapse", E("subsets", L(abcd, 4))))), E(how_many(subsets(abcd, 4))))
           && "sixteen of four");
    mt_atom *twelve[] = { mt_num(1), mt_num(2) }, *xy[] = { mt_sym("x"), mt_sym("y") };
    mt_atom *const *sets[] = { twelve, xy };
    const size_t both[] = { 2, 2 }, none_second[] = { 2, 0 };
    assert(answers_are(mt_eval(m, E("collapse", E("tuples", E(L(twelve, 2), L(xy, 2))))), E(collected(product(sets, both, 2))))
           && "the Cartesian product");
    assert(answers_are(mt_eval(m, E("collapse", E("tuples", E(L(twelve, 2), mt_unit())))), E(collected(product(sets, none_second, 2))))
           && "an empty set empties it");
    assert(answers_are(mt_eval(m, E("collapse", E("tuples", mt_unit()))), E(collected(product(sets, both, 0)))) && "no sets, one empty tuple");
    mt_atom *bits[] = { mt_num(0), mt_num(1) };
    assert(answers_are(mt_eval(m, E("collapse", E("cartesian-power", L(bits, 2), 2))), E(collected(power(bits, 2, 2)))) && "a set to a power");
    assert(answers_are(mt_eval(m, E("size-atom", E("collapse", E("cartesian-power", L(bits, 2), 3)))), E(how_many(power(bits, 2, 3))))
           && "eight binary strings of three");
    assert(answers_are(mt_eval(m, E("collapse", E("cartesian-power", L(ab, 2), 0))), E(collected(power(ab, 2, 0)))) && "the zeroth power");

    /* Counts. */
    assert(answers_are(mt_eval(m, E("factorial", 5)), E(factorial(5))) && "5!");
    assert(answers_are(mt_eval(m, E("factorial", 0)), E(factorial(0))) && "0!");
    assert(answers_are(mt_eval(m, E("binomial", 5, 2)), E(binomial(5, 2))) && "5 choose 2");
    assert(answers_are(mt_eval(m, E("binomial", 52, 5)), E(binomial(52, 5))) && "a card deal");
    assert(answers_are(mt_eval(m, E("binomial", 5, 0)), E(binomial(5, 0))) && "5 choose 0");
    assert(answers_are(mt_eval(m, E("binomial", 5, 9)), E(binomial(5, 9))) && "5 choose 9");
    assert(answers_are(mt_eval(m, E("permutation-count", 5, 2)), E(falling(5, 2))) && "5 permute 2");
    assert(answers_are(mt_eval(m, E("permutation-count", 5, 5)), E(factorial(5))) && "5 permute 5 is 5!");
    assert(answers_are(mt_eval(m, E("permutation-count", 5, 0)), E(falling(5, 0))) && "5 permute 0");
    mt_atom *minus_one = mt_num(-1);
    assert(answers_are(mt_eval(m, guarded(E("factorial", mt_keep(minus_one)))), E(verdict(a_count(minus_one)))) && "no factorial below zero");
    mt_drop(minus_one);

    /* Each count is its enumeration's size. */
    assert(answers_are(mt_eval(m, E("==", E("size-atom", E("collapse", E("permutations", L(abc, 3)))), E("factorial", 3))), E(B(same(mt_num(how_many(permutations(abc, 3))), factorial(3)))))
           && "orderings");
    assert(answers_are(mt_eval(m, E("==", E("size-atom", E("collapse", E("chooseK", L(abcd, 4), 2))), E("binomial", 4, 2))), E(B(same(mt_num(how_many(combinations(abcd, 4, 2))), binomial(4, 2)))))
           && "pairs of four");
    assert(answers_are(mt_eval(m, E("==", E("size-atom", E("collapse", E("subsets", L(abc, 3)))), E("pow-math", 2, 3))), E(B(how_many(subsets(abc, 3)) == 1 << 3)))
           && "subsets");
    assert(answers_are(mt_eval(m, E("==", E("size-atom", E("collapse", E("cartesian-power", L(ab, 2), 3))), E("pow-math", 2, 3))), E(B(how_many(power(ab, 2, 3)) == 1 << 3)))
           && "powers");

    /* Quoted expressions stay data: quote keeps the engine from evaluating
       what C hands it, and C's own answers are data already. */
    mt_atom *sum = E("+", 1, 2), *error = E("Error", "a", "b");
    mt_atom *sum_a[] = { sum, abcd[0] }, *sum_error[] = { sum, error }, *error_set[] = { sum, error }, *just_a[] = { abcd[0] };
    assert(answers_are(mt_eval(m, E("choose2", E("quote", L(sum_a, 2)))), E(combinations(sum_a, 2, 2).item[0])) && "choose2 keeps a sum a sum");
    assert(answers_are(mt_eval(m, E("chooseKl", E("quote", L(sum_a, 2)), 1)), E(collected(combinations(sum_a, 2, 1)))) && "chooseKl too");
    assert(answers_are(mt_eval(m, E("takeK", 1, E("quote", L(sum_a, 2)))), E(L(sum_a, 1))) && "takeK too");
    assert(answers_are(mt_eval(m, E("collapse", E("permutations", E("quote", L(sum_error, 2))))), E(collected(permutations(sum_error, 2))))
           && "permutations too");
    assert(answers_are(mt_eval(m, E("collapse", E("subsets", E("quote", L(sum_a, 2))))), E(collected(subsets(sum_a, 2)))) && "subsets too");
    mt_atom *const *quoted_sets[] = { error_set, just_a };
    const size_t quoted_sizes[] = { 2, 1 };
    assert(answers_are(mt_eval(m, E("collapse", E("tuples", E("quote", E(L(error_set, 2), L(just_a, 1)))))), E(collected(product(quoted_sets, quoted_sizes, 2))))
           && "tuples too");

    /* Laziness, bounds and refusals. */
    mt_atom *hundred[100];
    for (int64_t i = 0; i < 100; i++) hundred[i] = mt_num(i);
    assert(answers_are(mt_eval(m, E("once", E("chooseK", E("collapse", E("range", 0, 100)), 50))), E(L(hundred, 50)))
           && "the first of 10^29 choices, without the rest");
    for (size_t i = 0; i < 100; i++) mt_drop(hundred[i]);
    /* An empty base has no tuple at any positive power, so C needs no more
       than one wheel to know it. */
    assert(answers_are(mt_eval(m, E("collapse", E("cartesian-power", mt_unit(),
                                                  mt_bigint("9999999999999999999999999999999")))), E(collected(power(NULL, 0, 1))))
           && "an empty set to any power");
    mt_atom *three = mt_num(3), *one_point_oh = mt_real(1.0);
    assert(answers_are(mt_eval(m, guarded(E("tuples", E(mt_unit(), mt_keep(three))))), E(verdict(a_set(three))))
           && "a set that is not a list");
    assert(answers_are(mt_eval(m, guarded(E("cartesian-power", L(bits, 2), mt_keep(one_point_oh)))), E(verdict(a_count(one_point_oh))))
           && "a fractional power");
    assert(answers_are(mt_eval(m, guarded(E("factorial", mt_keep(one_point_oh)))), E(verdict(a_count(one_point_oh))))
           && "a fractional factorial");
    mt_drop(three);
    mt_drop(one_point_oh);
    assert(answers_are(mt_eval(m, E("binomial", 100, 50)), E(binomial(100, 50))) && "100 choose 50, past 64 bits");
    assert(answers_are(mt_eval(m, E("collapse", E("range-step", 0.5, 2.0, 1))), E(collected(stepped(0.5, 2.0, 1, true)))) && "a float range");
    assert(answers_are(mt_eval(m, E("collapse", guarded(E("range-step", 1.0e20, 1.0e21, 1)))), E(stalled(1.0e20, 1.0e21, 1)))
           && "a float range that stalls");

    for (size_t i = 0; i < COUNT(abcd); i++) mt_drop(abcd[i]);
    mt_drop(twelve[0]); mt_drop(twelve[1]); mt_drop(xy[0]); mt_drop(xy[1]); mt_drop(bits[0]); mt_drop(bits[1]);
    mt_drop(sum);
    mt_drop(error);
    mt_close(m);
    return 0;
}
