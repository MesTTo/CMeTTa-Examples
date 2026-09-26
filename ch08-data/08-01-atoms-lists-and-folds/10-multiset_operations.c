/* Purpose: the multiset operations, computed twice. unique-atom,
 *   union-atom, intersection-atom and subtraction-atom run in the engine,
 *   and C runs the same algebra over the expressions' children with mt_eq:
 *   first occurrences, concatenation, and copy-for-copy cancellation in the
 *   left side's order. Every engine answer must be C's.
 * Guarantees: all eight claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>

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

static mt_atom *unique(const mt_atom *a)
{
    mt_atom *out[MOST];
    size_t n = 0;
    for (size_t i = 0; i < mt_len(a); i++) {
        bool seen = false;
        for (size_t j = 0; j < n && !seen; j++) seen = mt_eq(out[j], mt_at(a, i));
        if (!seen) out[n++] = mt_keep(mt_at(a, i));
    }
    return mt_exprv(n, out);
}

static mt_atom *union_of(const mt_atom *a, const mt_atom *b)
{
    mt_atom *out[MOST];
    size_t n = 0;
    for (size_t i = 0; i < mt_len(a); i++) out[n++] = mt_keep(mt_at(a, i));
    for (size_t i = 0; i < mt_len(b); i++) out[n++] = mt_keep(mt_at(b, i));
    return mt_exprv(n, out);
}

/* a's children in order, kept when (intersection) or unless (subtraction)
   b still holds an unused copy of them. */
static mt_atom *against(const mt_atom *a, const mt_atom *b, bool keep_shared)
{
    mt_atom *out[MOST];
    bool used[MOST] = { false };
    size_t n = 0;
    for (size_t i = 0; i < mt_len(a); i++) {
        bool found = false;
        for (size_t j = 0; j < mt_len(b) && !found; j++)
            if (!used[j] && mt_eq(mt_at(a, i), mt_at(b, j))) used[j] = found = true;
        if (found == keep_shared) out[n++] = mt_keep(mt_at(a, i));
    }
    return mt_exprv(n, out);
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    mt_atom *repeated = E("a", "b", "c", "d", "d");
    mt_atom *left = E("a", "b", "b", "c"), *right = E("b", "c", "c", "d");
    mt_atom *wide = E("a", "b", "c", "c"), *wider = E("b", "c", "c", "c", "d"), *narrow = E("b", "c", "d");
    mt_atom *thrice = E("a", "a", "a"), *once = E("a"), *two = E("a", "b");

    assert(answers_are(mt_eval(m, E("unique-atom", mt_keep(repeated))), E(unique(repeated))) && "unique-atom");
    assert(answers_are(mt_eval(m, E("union-atom", mt_keep(left), mt_keep(right))), E(union_of(left, right))) && "union-atom");
    assert(answers_are(mt_eval(m, E("intersection-atom", mt_keep(wide), mt_keep(wider))), E(against(wide, wider, true)))
           && "intersection-atom");
    assert(answers_are(mt_eval(m, E("subtraction-atom", mt_keep(left), mt_keep(right))), E(against(left, right, false)))
           && "subtraction-atom");
    assert(answers_are(mt_eval(m, E("intersection-atom", mt_keep(wide), mt_keep(narrow))), E(against(wide, narrow, true)))
           && "intersection with fewer copies");
    assert(answers_are(mt_eval(m, E("intersection-atom", mt_keep(thrice), mt_keep(once))), E(against(thrice, once, true)))
           && "one copy in common");
    assert(answers_are(mt_eval(m, E("subtraction-atom", mt_keep(thrice), mt_keep(once))), E(against(thrice, once, false)))
           && "one copy cancelled");
    mt_atom *none = mt_unit();
    assert(answers_are(mt_eval(m, E("intersection-atom", mt_keep(two), mt_keep(none))), E(against(two, none, true)))
           && "nothing in common with nothing");
    mt_atom *all[] = { repeated, left, right, wide, wider, narrow, thrice, once, two, none };
    for (size_t i = 0; i < sizeof all / sizeof *all; i++) mt_drop(all[i]);
    mt_close(m);
    return 0;
}
