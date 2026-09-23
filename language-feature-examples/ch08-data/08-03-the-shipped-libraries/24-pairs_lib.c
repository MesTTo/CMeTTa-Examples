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
 * Guarantees: all thirty-seven claims of the original hold [tested: make
 *   twins; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

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
    metta *m = open_engine();
    require("import lib_pairs", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_pairs")))));

    mt_atom *none = mt_unit();
    mt_atom *sales = E(E("sydney", 120), E("perth", 90), E("sydney", 30)), *broken = E(E("a", 1), E("b")), *seven = mt_num(7);
    check_answers("a relation", mt_eval(m, E("pairs-is", mt_keep(sales))), B(is_relation(sales)));
    check_answers("a one-element row is no pair", mt_eval(m, E("pairs-is", mt_keep(broken))), B(is_relation(broken)));
    check_answers("the empty relation", mt_eval(m, E("pairs-is", mt_unit())), B(is_relation(none)));
    check_answers("a number is no relation", mt_eval(m, E("pairs-is", mt_keep(seven))), B(is_relation(seven)));

    check_answers("pairs-keys keeps order and repeats", mt_eval(m, E("pairs-keys", mt_keep(sales))), side(sales, 0));
    check_answers("pairs-values", mt_eval(m, E("pairs-values", mt_keep(sales))), side(sales, 1));
    check_answers("no keys of nothing", mt_eval(m, E("pairs-keys", mt_unit())), side(none, 0));

    mt_atom *converse = swapped(sales);
    check_answers("pairs-swap", mt_eval(m, E("pairs-swap", mt_keep(sales))), mt_keep(converse));
    check_answers("swapping twice is the relation", mt_eval(m, E("pairs-swap", E("pairs-swap", mt_keep(sales)))), swapped(converse));

    mt_atom *letters = E(E("b", 1), E("a", 2), E("b", 0), E("a", 1));
    check_answers("a stable sort by key", mt_eval(m, E("pairs-sort-by-key", mt_keep(sales))), sorted_by(sales, 0));
    check_answers("a stable sort by value", mt_eval(m, E("pairs-sort-by-value", mt_keep(sales))), sorted_by(sales, 1));
    check_answers("equal keys keep their order", mt_eval(m, E("pairs-sort-by-key", mt_keep(letters))), sorted_by(letters, 0));

    mt_atom *single = E(E("a", 1)), *groups = grouped(sales);
    check_answers("pairs-group", mt_eval(m, E("pairs-group", mt_keep(sales))), mt_keep(groups));
    check_answers("no groups of nothing", mt_eval(m, E("pairs-group", mt_unit())), grouped(none));
    check_answers("one group", mt_eval(m, E("pairs-group", mt_keep(single))), grouped(single));
    check_answers("ungrouping sorts by key", mt_eval(m, E("pairs-ungroup", E("pairs-group", mt_keep(sales)))), ungrouped(groups));
    mt_atom *spread = E(E("a", E(1, 2)), E("b", mt_unit()));
    check_answers("a key with no values contributes nothing", mt_eval(m, E("pairs-ungroup", mt_keep(spread))), ungrouped(spread));
    check_answers("no rows of nothing", mt_eval(m, E("pairs-ungroup", mt_unit())), ungrouped(none));

    /* A lookup answers once per value; an absent key answers nothing. */
    mt_atom *out[MOST], *sydney = S("sydney"), *perth = S("perth"), *darwin = S("darwin"), *city = V("city");
    check_answers_("a key with two values", mt_eval(m, E("pairs-lookup", mt_keep(sales), mt_keep(sydney))), lookup(sales, sydney, out), out);
    check_answers_("a key with one", mt_eval(m, E("pairs-lookup", mt_keep(sales), mt_keep(perth))), lookup(sales, perth, out), out);
    check_answers_("an absent key", mt_eval(m, E("pairs-lookup", mt_keep(sales), mt_keep(darwin))), lookup(sales, darwin, out), out);
    check_answers("no answer is what an if reads",
                  mt_eval(m, E("if", E("==", mt_unit(), E("collapse", E("pairs-lookup", mt_keep(sales), mt_keep(darwin)))), "none", "found")),
                  S(lookup(sales, darwin, NULL) == 0 ? "none" : "found"));
    check_answers("an answer too",
                  mt_eval(m, E("if", E("==", mt_unit(), E("collapse", E("pairs-lookup", mt_keep(sales), mt_keep(perth)))), "none", "found")),
                  S(lookup(sales, perth, NULL) == 0 ? "none" : "found"));
    check_answers_("a variable key finds only itself", mt_eval(m, E("pairs-lookup", mt_keep(sales), mt_keep(city))), lookup(sales, city, out), out);

    /* Refusals name the element that is no pair. */
    mt_atom *bare = E(E("a", 1), "b"), *three = E(E("a", 1), E("b", 2, 3)), *flat = E(E("a", 1));
    check_answers("keys of a non-relation", mt_eval(m, guarded(E("pairs-keys", mt_keep(bare)))), verdict(computed(side(bare, 0))));
    check_answers("a group of a non-relation", mt_eval(m, guarded(E("pairs-group", mt_keep(three)))), verdict(computed(grouped(three))));
    check_answers("a lookup in a number", mt_eval(m, guarded(E("pairs-lookup", mt_keep(seven), "a"))), verdict(is_relation(seven)));
    check_answers("ungrouping values that are no collection", mt_eval(m, guarded(E("pairs-ungroup", mt_keep(flat)))),
                  verdict(computed(ungrouped(flat))));

    /* Literal data stays data. */
    mt_atom *arith = E("+", 1, 2), *error = E("Error", "a", "b"), *heads = E(E("+", "a"), E("Error", "b")),
            *data = E(E("a", mt_keep(arith)), E("b", mt_keep(error))), *mixed = E(E("b", mt_keep(arith)), E("a", 2), E("b", mt_keep(error))),
            *regrouped = E(E("a", mt_unit()), E("b", E(mt_keep(arith), mt_keep(error)))),
            *same_key = E(E("a", mt_keep(arith)), E("a", mt_keep(error))), *key = V("key"), *by_var = E(E(mt_keep(key), 7), E("a", 8)), *a = S("a");
    check_answers("a variable is no relation", mt_eval(m, E("pairs-is", mt_keep(city))), B(is_relation(city)));
    check_answers("runnable heads are data", mt_eval(m, E("pairs-is", E("quote", mt_keep(heads)))), B(is_relation(heads)));
    check_answers("and keys", mt_eval(m, E("pairs-keys", E("quote", mt_keep(heads)))), side(heads, 0));
    check_answers("and values", mt_eval(m, E("pairs-values", E("quote", mt_keep(data)))), side(data, 1));
    check_answers("and a converse", mt_eval(m, E("pairs-swap", E("quote", mt_keep(data)))), swapped(data));
    check_answers("and groups", mt_eval(m, E("pairs-group", E("quote", mt_keep(mixed)))), grouped(mixed));
    check_answers("and rows", mt_eval(m, E("pairs-ungroup", E("quote", mt_keep(regrouped)))), ungrouped(regrouped));
    check_answers_("and lookups", mt_eval(m, E("pairs-lookup", E("quote", mt_keep(same_key)), mt_keep(a))), lookup(same_key, a, out), out);
    check_answers_("a variable key finds its own row", mt_eval(m, E("pairs-lookup", E("quote", mt_keep(by_var)), mt_keep(key))),
                   lookup(by_var, key, out), out);

    mt_atom *held[] = { none, sales, broken, seven, converse, letters, single, groups, spread, sydney, perth, darwin, city, bare, three,
                        flat, arith, error, heads, data, mixed, regrouped, same_key, key, by_var, a };
    for (size_t i = 0; i < sizeof held / sizeof *held; i++) mt_drop(held[i]);
    return done(m);
}
