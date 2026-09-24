/* Purpose: primality under hyperpose. prime? is a C function deciding
 *   primality by deterministic Miller-Rabin in 128-bit arithmetic, so each of
 *   the original's branches, cheap or expensive for its trial division, is a
 *   few hundred multiplications in C; find-divisor, that trial division, is
 *   published beside it under its own name. hyperpose runs the checks on the
 *   engine's threads: all four answer the True C computes, and once answers
 *   the first branch to finish, True whichever it is, since C finds every
 *   candidate prime. The answers of a hyperpose over a list arrive in
 *   completion order, so C sorts them with qsort under mt_order, the engine's
 *   standard order, where the original asks msort.
 * Guarantees: all three claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

__extension__ typedef unsigned __int128 wide;

static uint64_t mulmod(uint64_t a, uint64_t b, uint64_t n) { return (uint64_t)((wide)a * b % n); }

static uint64_t powmod(uint64_t a, uint64_t e, uint64_t n)
{
    uint64_t r = 1;
    for (a %= n; e; e >>= 1, a = mulmod(a, a, n))
        if (e & 1) r = mulmod(r, a, n);
    return r;
}

/* Whether n is prime, exactly for every n below 2^64: the least strong
   pseudoprime to all of the first twelve prime bases is
   318665857834031151167461 [source: OEIS A014233, a(12), and J. Sorenson and
   J. Webster, Strong pseudoprimes to twelve prime bases, Math. Comp. 86
   (2017) 985-1003, doi:10.1090/mcom/3134]. Time: 12 log2(n) modular
   squarings. */
static bool is_prime(uint64_t n)
{
    static const uint64_t bases[] = { 2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37 };
    const size_t count = sizeof bases / sizeof *bases;
    if (n < 2) return false;
    for (size_t i = 0; i < count; i++)
        if (n % bases[i] == 0) return n == bases[i];
    uint64_t d = n - 1;
    int s = 0;
    for (; !(d & 1); d >>= 1) s++;
    for (size_t i = 0; i < count; i++) {
        uint64_t x = powmod(bases[i], d, n);
        bool composite = x != 1 && x != n - 1;
        for (int r = 1; r < s && composite; r++) composite = (x = mulmod(x, x, n)) != n - 1;
        if (composite) return false;
    }
    return true;
}

/* The original's trial division: the least divisor of n from t upward, or n
   once t*t passes it. Time: sqrt(n) - t divisions at most. */
static int64_t find_divisor(int64_t n, int64_t t)
{
    for (; t * t <= n; t++)
        if (n % t == 0) return t;
    return n;
}

static mt_status prime_op(mt_call *call, void *user)
{
    (void)user;
    return mt_answer(call, B(is_prime((uint64_t)mt_int(mt_arg(call, 0)))));
}

static mt_status find_divisor_op(mt_call *call, void *user)
{
    (void)user;
    return mt_answer(call, N(find_divisor(mt_int(mt_arg(call, 0)), mt_int(mt_arg(call, 1)))));
}

static mt_atom *primality(int64_t x) { return E("prime?", x); }
static mt_atom *verdict(int64_t x) { return B(is_prime((uint64_t)x)); }

/* The expression of f over each of the numbers. */
static mt_atom *each_of(const int64_t *xs, size_t n, mt_atom *(*f)(int64_t))
{
    mt_atom **items = malloc((n + 1) * sizeof *items);
    require("room for the expression", items != NULL);
    for (size_t i = 0; i < n; i++) items[i] = f(xs[i]);
    mt_atom *out = mt_exprv(n, items);
    free(items);
    return out;
}

int main(void)
{
    metta *m = open_engine();
    require("publish prime?", mt_def(m, (mt_op){ .name = "prime?", .arity = 1, .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = prime_op }));
    require("publish find-divisor", mt_def(m, (mt_op){ .name = "find-divisor", .arity = 2, .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = find_divisor_op }));

    const int64_t listed[] = { 3, 1, 2 };
    const size_t listed_n = sizeof listed / sizeof *listed;
    mt_list answers = mt_all(mt_eval(m, E("let", V("xs"), mt_array(listed_n, listed), E("hyperpose", V("xs")))));
    qsort(answers.items, answers.len, sizeof *answers.items, mt_order);
    mt_atom **sorted = malloc(listed_n * sizeof *sorted);
    require("room for the list", sorted != NULL);
    for (size_t i = 0; i < listed_n; i++) sorted[i] = N(listed[i]);
    qsort(sorted, listed_n, sizeof *sorted, mt_order);
    check_list_("hyperpose over a list held in a variable", answers, listed_n, sorted);
    free(sorted);

    const int64_t cheap[] = { 5353725700019, 5378181100003, 5421844300001, 5473443100001 };
    const size_t cheap_n = sizeof cheap / sizeof *cheap;
    check_answers("every branch runs to the end", mt_eval(m, E("collapse", E("hyperpose", each_of(cheap, cheap_n, primality)))),
                  each_of(cheap, cheap_n, verdict));

    const int64_t mixed[] = { 535372570000000063, 537818110000000001, 5421844300001, 547344310000000013 };
    const size_t mixed_n = sizeof mixed / sizeof *mixed;
    bool all_prime = true;
    for (size_t i = 0; i < mixed_n; i++) all_prime &= is_prime((uint64_t)mixed[i]);
    require("every candidate is prime, so any branch may finish first", all_prime);
    check_answers("once takes the first branch to finish", mt_eval(m, E("once", E("hyperpose", each_of(mixed, mixed_n, primality)))),
                  B(true));
    return done(m);
}
