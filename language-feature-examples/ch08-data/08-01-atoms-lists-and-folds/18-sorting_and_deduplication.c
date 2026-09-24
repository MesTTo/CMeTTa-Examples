/* Purpose: four ways to tidy a collection, each done by C beside the
 *   engine. sort is qsort with mt_order, the engine's standard order, then
 *   equal neighbours dropped; sort-atom and msort are the same qsort keeping
 *   them; list_to_set keeps first occurrences in order; exclude-item is a
 *   filter; and over an answer stream unique keeps exact duplicates apart
 *   while alpha-unique merges renamings, a C loop with mt_eq or mt_alpha_eq.
 * Guarantees: all fifteen claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

enum { MOST = 16 };

/* The children of `e`, sorted by the standard order, equals dropped or kept. */
static mt_atom *sorted(const mt_atom *e, bool drop_equals)
{
    mt_atom *items[MOST];
    size_t n = mt_len(e), kept = 0;
    for (size_t i = 0; i < n; i++) items[i] = mt_keep(mt_at(e, i));
    qsort(items, n, sizeof *items, mt_order);
    for (size_t i = 0; i < n; i++) {
        if (drop_equals && kept > 0 && mt_compare(items[kept - 1], items[i]) == 0) mt_drop(items[i]);
        else items[kept++] = items[i];
    }
    return mt_exprv(kept, items);
}

/* The first of each class, `same` deciding the classes. */
static mt_atom *first_of_each(size_t n, const mt_atom *const *items, bool (*same)(const mt_atom *, const mt_atom *))
{
    mt_atom *kept[MOST];
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        bool seen = false;
        for (size_t j = 0; j < k && !seen; j++) seen = same(kept[j], items[i]);
        if (!seen) kept[k++] = mt_keep(items[i]);
    }
    return mt_exprv(k, kept);
}

static mt_atom *list_to_set(const mt_atom *e) { return first_of_each(mt_len(e), mt_children(e), mt_eq); }

static mt_atom *excluding(const mt_atom *item, const mt_atom *e)
{
    mt_atom *kept[MOST];
    size_t k = 0;
    for (size_t i = 0; i < mt_len(e); i++)
        if (!mt_eq(item, mt_at(e, i))) kept[k++] = mt_keep(mt_at(e, i));
    return mt_exprv(k, kept);
}

/* The engine's collapsed stream beside C's first-of-each over the same
   alternatives: one claim that the two are alpha-equal. */
static void deduped(metta *m, const char *claim, const char *op, mt_atom *alternatives,
                    bool (*same)(const mt_atom *, const mt_atom *))
{
    mt_atom *mine = first_of_each(mt_len(alternatives), mt_children(alternatives), same);
    mt_atom *engine = mt_one(mt_eval(m, E("collapse", E(op, E("superpose", alternatives)))));
    check(claim, engine && mt_alpha_eq(engine, mine));
    mt_drop(engine);
    mt_drop(mine);
}

int main(void)
{
    metta *m = open_engine();
    mt_atom *digits = E(3, 1, 2, 1), *mixed = E("b", "a", 10, 2, T("s")), *letters = E("b", "a", "b", "c", "a");

    check_answers("sort drops equals", mt_eval(m, E("sort", mt_keep(digits))), sorted(digits, true));
    check_answers("sort-atom keeps them", mt_eval(m, E("sort-atom", mt_keep(digits))), sorted(digits, false));
    check_answers("msort keeps them", mt_eval(m, E("msort", mt_keep(digits))), sorted(digits, false));
    check_answers("sorting nothing", mt_eval(m, E("sort", mt_unit())), mt_unit());
    check_answers("the standard order: numbers, strings, names", mt_eval(m, E("sort", mt_keep(mixed))), sorted(mixed, true));

    check_answers("list_to_set keeps the first of each", mt_eval(m, E("list_to_set", mt_keep(letters))), list_to_set(letters));
    check_answers("in their own order", mt_eval(m, E("list_to_set", mt_keep(digits))), list_to_set(digits));
    check_answers("of nothing", mt_eval(m, E("list_to_set", mt_unit())), mt_unit());

    mt_atom *two = N(2), *x = S("x"), *one = N(1), *list = E(1, 2, 3, 2), *plain = E(1, 2, 3), *ones = E(1, 1, 1);
    check_answers("exclude-item drops every copy", mt_eval(m, E("exclude-item", mt_keep(two), mt_keep(list))), excluding(two, list));
    check_answers("and keeps a list without it", mt_eval(m, E("exclude-item", mt_keep(x), mt_keep(plain))), excluding(x, plain));
    check_answers("down to nothing", mt_eval(m, E("exclude-item", mt_keep(one), mt_keep(ones))), excluding(one, ones));

    deduped(m, "unique keeps renamings apart", "unique", E(E("f", V("x")), E("f", V("y")), E("g", V("z"))), mt_eq);
    deduped(m, "alpha-unique merges them", "alpha-unique", E(E("f", V("x")), E("f", V("y")), E("g", V("z"))), mt_alpha_eq);
    deduped(m, "on ground data they agree", "alpha-unique", E(1, 2, 1, 3), mt_alpha_eq);
    deduped(m, "unique on the same", "unique", E(1, 2, 1, 3), mt_eq);
    deduped(m, "a variable is not the ground term it would unify with", "alpha-unique",
            E(E("f", V("x")), E("f", V("y")), E("f", 1)), mt_alpha_eq);
    mt_atom *all[] = { digits, mixed, letters, two, x, one, list, plain, ones };
    for (size_t i = 0; i < sizeof all / sizeof *all; i++) mt_drop(all[i]);
    return done(m);
}
