/* Purpose: four ways to tidy a collection, each done by C beside the
 *   engine. sort is qsort with mt_order, the engine's standard order, then
 *   equal neighbours dropped; sort-atom and msort are the same qsort keeping
 *   them; list_to_set keeps first occurrences in order; exclude-item is a
 *   filter; and over an answer stream unique keeps exact duplicates apart
 *   while alpha-unique merges renamings, a C loop with mt_eq or mt_alpha_eq.
 * Guarantees: all fifteen claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>

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
    assert(engine && mt_alpha_eq(engine, mine) && claim);
    mt_drop(engine);
    mt_drop(mine);
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    mt_atom *digits = E(3, 1, 2, 1), *mixed = E("b", "a", 10, 2, T("s")), *letters = E("b", "a", "b", "c", "a");

    assert(answers_are(mt_eval(m, E("sort", mt_keep(digits))), E(sorted(digits, true))) && "sort drops equals");
    assert(answers_are(mt_eval(m, E("sort-atom", mt_keep(digits))), E(sorted(digits, false))) && "sort-atom keeps them");
    assert(answers_are(mt_eval(m, E("msort", mt_keep(digits))), E(sorted(digits, false))) && "msort keeps them");
    assert(answers_are(mt_eval(m, E("sort", mt_unit())), E(mt_unit())) && "sorting nothing");
    assert(answers_are(mt_eval(m, E("sort", mt_keep(mixed))), E(sorted(mixed, true))) && "the standard order: numbers, strings, names");

    assert(answers_are(mt_eval(m, E("list_to_set", mt_keep(letters))), E(list_to_set(letters))) && "list_to_set keeps the first of each");
    assert(answers_are(mt_eval(m, E("list_to_set", mt_keep(digits))), E(list_to_set(digits))) && "in their own order");
    assert(answers_are(mt_eval(m, E("list_to_set", mt_unit())), E(mt_unit())) && "of nothing");

    mt_atom *two = N(2), *x = S("x"), *one = N(1), *list = E(1, 2, 3, 2), *plain = E(1, 2, 3), *ones = E(1, 1, 1);
    assert(answers_are(mt_eval(m, E("exclude-item", mt_keep(two), mt_keep(list))), E(excluding(two, list))) && "exclude-item drops every copy");
    assert(answers_are(mt_eval(m, E("exclude-item", mt_keep(x), mt_keep(plain))), E(excluding(x, plain))) && "and keeps a list without it");
    assert(answers_are(mt_eval(m, E("exclude-item", mt_keep(one), mt_keep(ones))), E(excluding(one, ones))) && "down to nothing");

    deduped(m, "unique keeps renamings apart", "unique", E(E("f", V("x")), E("f", V("y")), E("g", V("z"))), mt_eq);
    deduped(m, "alpha-unique merges them", "alpha-unique", E(E("f", V("x")), E("f", V("y")), E("g", V("z"))), mt_alpha_eq);
    deduped(m, "on ground data they agree", "alpha-unique", E(1, 2, 1, 3), mt_alpha_eq);
    deduped(m, "unique on the same", "unique", E(1, 2, 1, 3), mt_eq);
    deduped(m, "a variable is not the ground term it would unify with", "alpha-unique",
            E(E("f", V("x")), E("f", V("y")), E("f", 1)), mt_alpha_eq);
    mt_atom *all[] = { digits, mixed, letters, two, x, one, list, plain, ones };
    for (size_t i = 0; i < sizeof all / sizeof *all; i++) mt_drop(all[i]);
    mt_close(m);
    return 0;
}
