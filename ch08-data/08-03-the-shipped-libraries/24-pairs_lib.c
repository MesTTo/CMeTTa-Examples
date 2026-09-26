/* Purpose: lib_pairs, a collection of (Key Value) pairs read as a relation,
 *   held against the same readings written in C over the pairs' array.
 *   Projections and the converse keep order and repeats; both sorts are an
 *   insertion sort by mt_compare, the engine's standard order, which is
 *   stable, so equal keys keep their relative order; a group is a run of
 *   equal keys after that sort, which is why sydney is gathered once however
 *   its rows were spread; ungrouping lays each key beside each of its values;
 *   and a lookup compares keys as terms, so a variable finds only itself.
 *   Where the library refuses an element that is no pair, C's reading
 *   refuses it too.
 * Guarantees: all thirty-seven claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>

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

enum { MOST = 16 };

static bool is_pair(const mt_atom *x) { return mt_kind_of(x) == MT_EXPR && mt_len(x) == 2; }

static bool is_relation(const mt_atom *r)
{
    if (mt_kind_of(r) != MT_EXPR) return false;
    for (size_t i = 0; i < mt_len(r); i++)
        if (!is_pair(mt_at(r, i))) return false;
    return true;
}

/* One side of every pair: 0 the keys, 1 the values. NULL when an element is
   no pair. */
static mt_atom *side(const mt_atom *r, size_t which)
{
    if (!is_relation(r)) return NULL;
    mt_atom *out[MOST];
    for (size_t i = 0; i < mt_len(r); i++) out[i] = mt_keep(mt_at(mt_at(r, i), which));
    return mt_exprv(mt_len(r), out);
}

static mt_atom *swapped(const mt_atom *r)
{
    mt_atom *out[MOST];
    for (size_t i = 0; i < mt_len(r); i++) out[i] = E(mt_keep(mt_at(mt_at(r, i), 1)), mt_keep(mt_at(mt_at(r, i), 0)));
    return mt_exprv(mt_len(r), out);
}

/* The pairs ordered by one side, stably: an insertion sort moves a pair
   only past pairs strictly greater. Time: n^2 / 2 comparisons at worst. */
static size_t sorted(const mt_atom *r, size_t by, const mt_atom **out)
{
    size_t n = mt_len(r);
    for (size_t i = 0; i < n; i++) {
        size_t j = i;
        for (; j > 0 && mt_compare(mt_at(out[j - 1], by), mt_at(mt_at(r, i), by)) > 0; j--) out[j] = out[j - 1];
        out[j] = mt_at(r, i);
    }
    return n;
}

static mt_atom *sorted_by(const mt_atom *r, size_t by)
{
    const mt_atom *order[MOST];
    mt_atom *out[MOST];
    size_t n = sorted(r, by, order);
    for (size_t i = 0; i < n; i++) out[i] = mt_keep(order[i]);
    return mt_exprv(n, out);
}

/* Every key once, in order, with all its values: runs of equal keys after a
   stable sort. NULL when an element is no pair. */
static mt_atom *grouped(const mt_atom *r)
{
    if (!is_relation(r)) return NULL;
    const mt_atom *order[MOST];
    mt_atom *groups[MOST];
    size_t n = sorted(r, 0, order), g = 0;
    for (size_t from = 0, to; from < n; from = to) {
        mt_atom *values[MOST];
        for (to = from; to < n && mt_compare(mt_at(order[to], 0), mt_at(order[from], 0)) == 0; to++)
            values[to - from] = mt_keep(mt_at(order[to], 1));
        groups[g++] = E(mt_keep(mt_at(order[from], 0)), mt_exprv(to - from, values));
    }
    return mt_exprv(g, groups);
}

/* Each key beside each of its values; NULL when a group's values are no
   collection. */
static mt_atom *ungrouped(const mt_atom *groups)
{
    if (!is_relation(groups)) return NULL;
    mt_atom *out[MOST];
    size_t n = 0;
    for (size_t g = 0; g < mt_len(groups); g++) {
        const mt_atom *key = mt_at(mt_at(groups, g), 0), *values = mt_at(mt_at(groups, g), 1);
        if (mt_kind_of(values) != MT_EXPR) {
            while (n) mt_drop(out[--n]);
            return NULL;
        }
        for (size_t v = 0; v < mt_len(values); v++) out[n++] = E(mt_keep(key), mt_keep(mt_at(values, v)));
    }
    return mt_exprv(n, out);
}

/* How many pairs have this term as key, in the relation's order, their
   values written to out unless it is NULL; require()s a relation, since each
   claim that looks up hands C one. */
static size_t lookup(const mt_atom *r, const mt_atom *key, mt_atom **out)
{
    require("a relation", is_relation(r));
    size_t n = 0;
    for (size_t i = 0; i < mt_len(r); i++)
        if (mt_compare(mt_at(mt_at(r, i), 0), key) == 0) {
            if (out) out[n] = mt_keep(mt_at(mt_at(r, i), 1));
            n++;
        }
    return n;
}

static mt_atom *verdict(bool holds) { return S(holds ? "fine" : "refused"); }
static mt_atom *guarded(mt_atom *goal) { return E("if-error", E("catch", goal), "refused", "fine"); }

static bool computed(mt_atom *value)
{
    mt_drop(value);
    return value != NULL;
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_pairs", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_pairs")))));

    mt_atom *none = mt_unit();
    mt_atom *sales = E(E("sydney", 120), E("perth", 90), E("sydney", 30)), *broken = E(E("a", 1), E("b")), *seven = mt_num(7);
    assert(answers_are(mt_eval(m, E("pairs-is", mt_keep(sales))), E(B(is_relation(sales)))) && "a relation");
    assert(answers_are(mt_eval(m, E("pairs-is", mt_keep(broken))), E(B(is_relation(broken)))) && "a one-element row is no pair");
    assert(answers_are(mt_eval(m, E("pairs-is", mt_unit())), E(B(is_relation(none)))) && "the empty relation");
    assert(answers_are(mt_eval(m, E("pairs-is", mt_keep(seven))), E(B(is_relation(seven)))) && "a number is no relation");

    assert(answers_are(mt_eval(m, E("pairs-keys", mt_keep(sales))), E(side(sales, 0))) && "pairs-keys keeps order and repeats");
    assert(answers_are(mt_eval(m, E("pairs-values", mt_keep(sales))), E(side(sales, 1))) && "pairs-values");
    assert(answers_are(mt_eval(m, E("pairs-keys", mt_unit())), E(side(none, 0))) && "no keys of nothing");

    mt_atom *converse = swapped(sales);
    assert(answers_are(mt_eval(m, E("pairs-swap", mt_keep(sales))), E(mt_keep(converse))) && "pairs-swap");
    assert(answers_are(mt_eval(m, E("pairs-swap", E("pairs-swap", mt_keep(sales)))), E(swapped(converse))) && "swapping twice is the relation");

    mt_atom *letters = E(E("b", 1), E("a", 2), E("b", 0), E("a", 1));
    assert(answers_are(mt_eval(m, E("pairs-sort-by-key", mt_keep(sales))), E(sorted_by(sales, 0))) && "a stable sort by key");
    assert(answers_are(mt_eval(m, E("pairs-sort-by-value", mt_keep(sales))), E(sorted_by(sales, 1))) && "a stable sort by value");
    assert(answers_are(mt_eval(m, E("pairs-sort-by-key", mt_keep(letters))), E(sorted_by(letters, 0))) && "equal keys keep their order");

    mt_atom *single = E(E("a", 1)), *groups = grouped(sales);
    assert(answers_are(mt_eval(m, E("pairs-group", mt_keep(sales))), E(mt_keep(groups))) && "pairs-group");
    assert(answers_are(mt_eval(m, E("pairs-group", mt_unit())), E(grouped(none))) && "no groups of nothing");
    assert(answers_are(mt_eval(m, E("pairs-group", mt_keep(single))), E(grouped(single))) && "one group");
    assert(answers_are(mt_eval(m, E("pairs-ungroup", E("pairs-group", mt_keep(sales)))), E(ungrouped(groups))) && "ungrouping sorts by key");
    mt_atom *spread = E(E("a", E(1, 2)), E("b", mt_unit()));
    assert(answers_are(mt_eval(m, E("pairs-ungroup", mt_keep(spread))), E(ungrouped(spread))) && "a key with no values contributes nothing");
    assert(answers_are(mt_eval(m, E("pairs-ungroup", mt_unit())), E(ungrouped(none))) && "no rows of nothing");

    /* A lookup answers once per value; an absent key answers nothing. */
    mt_atom *out[MOST], *sydney = S("sydney"), *perth = S("perth"), *darwin = S("darwin"), *city = V("city");
    assert(answers_are(mt_eval(m, E("pairs-lookup", mt_keep(sales), mt_keep(sydney))), mt_exprv(lookup(sales, sydney, out), out)) && "a key with two values");
    assert(answers_are(mt_eval(m, E("pairs-lookup", mt_keep(sales), mt_keep(perth))), mt_exprv(lookup(sales, perth, out), out)) && "a key with one");
    assert(answers_are(mt_eval(m, E("pairs-lookup", mt_keep(sales), mt_keep(darwin))), mt_exprv(lookup(sales, darwin, out), out)) && "an absent key");
    assert(answers_are(mt_eval(m, E("if", E("==", mt_unit(), E("collapse", E("pairs-lookup", mt_keep(sales), mt_keep(darwin)))), "none", "found")), E(S(lookup(sales, darwin, NULL) == 0 ? "none" : "found")))
           && "no answer is what an if reads");
    assert(answers_are(mt_eval(m, E("if", E("==", mt_unit(), E("collapse", E("pairs-lookup", mt_keep(sales), mt_keep(perth)))), "none", "found")), E(S(lookup(sales, perth, NULL) == 0 ? "none" : "found")))
           && "an answer too");
    assert(answers_are(mt_eval(m, E("pairs-lookup", mt_keep(sales), mt_keep(city))), mt_exprv(lookup(sales, city, out), out)) && "a variable key finds only itself");

    /* Refusals name the element that is no pair. */
    mt_atom *bare = E(E("a", 1), "b"), *three = E(E("a", 1), E("b", 2, 3)), *flat = E(E("a", 1));
    assert(answers_are(mt_eval(m, guarded(E("pairs-keys", mt_keep(bare)))), E(verdict(computed(side(bare, 0))))) && "keys of a non-relation");
    assert(answers_are(mt_eval(m, guarded(E("pairs-group", mt_keep(three)))), E(verdict(computed(grouped(three))))) && "a group of a non-relation");
    assert(answers_are(mt_eval(m, guarded(E("pairs-lookup", mt_keep(seven), "a"))), E(verdict(is_relation(seven)))) && "a lookup in a number");
    assert(answers_are(mt_eval(m, guarded(E("pairs-ungroup", mt_keep(flat)))), E(verdict(computed(ungrouped(flat)))))
           && "ungrouping values that are no collection");

    /* Literal data stays data. */
    mt_atom *arith = E("+", 1, 2), *error = E("Error", "a", "b"), *heads = E(E("+", "a"), E("Error", "b")),
            *data = E(E("a", mt_keep(arith)), E("b", mt_keep(error))), *mixed = E(E("b", mt_keep(arith)), E("a", 2), E("b", mt_keep(error))),
            *regrouped = E(E("a", mt_unit()), E("b", E(mt_keep(arith), mt_keep(error)))),
            *same_key = E(E("a", mt_keep(arith)), E("a", mt_keep(error))), *key = V("key"), *by_var = E(E(mt_keep(key), 7), E("a", 8)), *a = S("a");
    assert(answers_are(mt_eval(m, E("pairs-is", mt_keep(city))), E(B(is_relation(city)))) && "a variable is no relation");
    assert(answers_are(mt_eval(m, E("pairs-is", E("quote", mt_keep(heads)))), E(B(is_relation(heads)))) && "runnable heads are data");
    assert(answers_are(mt_eval(m, E("pairs-keys", E("quote", mt_keep(heads)))), E(side(heads, 0))) && "and keys");
    assert(answers_are(mt_eval(m, E("pairs-values", E("quote", mt_keep(data)))), E(side(data, 1))) && "and values");
    assert(answers_are(mt_eval(m, E("pairs-swap", E("quote", mt_keep(data)))), E(swapped(data))) && "and a converse");
    assert(answers_are(mt_eval(m, E("pairs-group", E("quote", mt_keep(mixed)))), E(grouped(mixed))) && "and groups");
    assert(answers_are(mt_eval(m, E("pairs-ungroup", E("quote", mt_keep(regrouped)))), E(ungrouped(regrouped))) && "and rows");
    assert(answers_are(mt_eval(m, E("pairs-lookup", E("quote", mt_keep(same_key)), mt_keep(a))), mt_exprv(lookup(same_key, a, out), out)) && "and lookups");
    assert(answers_are(mt_eval(m, E("pairs-lookup", E("quote", mt_keep(by_var)), mt_keep(key))), mt_exprv(lookup(by_var, key, out), out))
           && "a variable key finds its own row");

    mt_atom *held[] = { none, sales, broken, seven, converse, letters, single, groups, spread, sydney, perth, darwin, city, bare, three,
                        flat, arith, error, heads, data, mixed, regrouped, same_key, key, by_var, a };
    for (size_t i = 0; i < sizeof held / sizeof *held; i++) mt_drop(held[i]);
    mt_close(m);
    return 0;
}
