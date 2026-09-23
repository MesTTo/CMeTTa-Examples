/* Purpose: a pattern's shape selects. &wuspace holds (wu) and (wu 42); ($x)
 *   is an expression of one element, so only (wu) matches it, with $x bound
 *   to wu, while a bare $x matches every atom. Bindings are read by name
 *   with mt_bound() and built into new terms in C, and what the space holds
 *   is sorted with qsort in the engine's order.
 * Guarantees: all five claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

/* Every answer, kept, in the engine's standard order. */
static mt_list sorted(mt_answers *answers)
{
    mt_list all = mt_all(answers);
    qsort(all.items, all.len, sizeof *all.items, mt_order);
    return all;
}

int main(void)
{
    metta *m = open_engine();
    mt_space *wu = mt_space_open(m, "&wuspace");
    require("open &wuspace", wu != NULL);
    require("store (wu)", mt_add(wu, E("wu")));
    require("store (wu 42)", mt_add(wu, E("wu", 42)));

    /* ($x) selects the one-element atom and binds its element. */
    check_answers("($x) matches only (wu)", mt_match(wu, E(V("x"))), E("wu"));
    mt_rows (row, mt_match(wu, E(V("x")))) {
        const mt_atom *x = mt_bound(row, "x");
        check_atom("(hu $x) is (hu wu)", E("hu", mt_keep(x)), E("hu", "wu"));
        check_atom("and $x alone is wu", mt_keep(x), S("wu"));
    }

    /* A bare variable matches every atom, so it answers the space itself. */
    mt_list every = sorted(mt_match(wu, V("x")));
    mt_list held = sorted(mt_atoms(wu));
    bool same = every.len == held.len;
    for (size_t i = 0; same && i < held.len; i++) same = mt_alpha_eq(every.items[i], held.items[i]);
    check("$x matches exactly what the space holds", same);
    mt_list_free(held);

    /* (wu $x) wraps each of them. */
    for (size_t i = 0; i < every.len; i++) every.items[i] = E("wu", every.items[i]);
    check_list("(wu $x) is ((wu (wu)) (wu (wu 42)))", every, E("wu", E("wu")), E("wu", E("wu", 42)));
    mt_space_close(wu);
    return done(m);
}
