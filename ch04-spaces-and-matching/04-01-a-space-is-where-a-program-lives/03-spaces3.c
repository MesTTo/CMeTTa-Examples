/* Purpose: a pattern's shape selects. &wuspace holds (wu) and (wu 42); ($x)
 *   is an expression of one element, so only (wu) matches it, with $x bound
 *   to wu, while a bare $x matches every atom. Bindings are read by name
 *   with mt_bound() and built into new terms in C, and what the space holds
 *   is sorted with qsort in the engine's order.
 * Guarantees: all five claims of the original hold
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

/* Whether an atom is want up to renaming variables; takes both, and shows
   them when they differ. */
static inline bool atom_is(mt_atom *got, mt_atom *want)
{
    bool holds = got && want && mt_alpha_eq(got, want);
    if (!holds) fprintf(stderr, "  got %s\n  want %s\n", got ? mt_show(got) : "nothing",
                        want ? mt_show(want) : "nothing");
    mt_drop(got);
    mt_drop(want);
    return holds;
}

/* Every answer, kept, in the engine's standard order. */
static mt_list sorted(mt_answers *answers)
{
    mt_list all = mt_all(answers);
    qsort(all.items, all.len, sizeof *all.items, mt_order);
    return all;
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    mt_space *wu = mt_space_open(m, "&wuspace");
    require("open &wuspace", wu != NULL);
    require("store (wu)", mt_add(wu, E("wu")));
    require("store (wu 42)", mt_add(wu, E("wu", 42)));

    /* ($x) selects the one-element atom and binds its element. */
    assert(answers_are(mt_match(wu, E(V("x"))), E(E("wu"))) && "($x) matches only (wu)");
    mt_rows (row, mt_match(wu, E(V("x")))) {
        const mt_atom *x = mt_bound(row, "x");
        assert(atom_is(E("hu", mt_keep(x)), E("hu", "wu")) && "(hu $x) is (hu wu)");
        assert(atom_is(mt_keep(x), S("wu")) && "and $x alone is wu");
    }

    /* A bare variable matches every atom, so it answers the space itself. */
    mt_list every = sorted(mt_match(wu, V("x")));
    mt_list held = sorted(mt_atoms(wu));
    bool same = every.len == held.len;
    for (size_t i = 0; same && i < held.len; i++) same = mt_alpha_eq(every.items[i], held.items[i]);
    assert(same && "$x matches exactly what the space holds");
    mt_list_free(held);

    /* (wu $x) wraps each of them. */
    for (size_t i = 0; i < every.len; i++) every.items[i] = E("wu", every.items[i]);
    assert(list_is(every, E(E("wu", E("wu")), E("wu", E("wu", 42)))) && "(wu $x) is ((wu (wu)) (wu (wu 42)))");
    mt_space_close(wu);
    mt_close(m);
    return 0;
}
