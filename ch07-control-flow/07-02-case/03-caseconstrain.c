/* Purpose: destructuring an expression. The engine's case binds the head of
 *   (1 2 3) through a cons pattern; C holds the same expression as an array
 *   of children, so its head is mt_at(e, 0) and the rest a view over the
 *   same children, which mt_expr_ref builds without copying them.
 * Guarantees: the original's claim holds, and C's destructuring agrees
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

static void drop_parent(void *parent) { mt_drop(parent); }

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    assert(answers_are(mt_eval(m, E("case", E(1, 2, 3), E(E(E("cons", V("h"), V("t")), V("h"))))), E(1))
           && "(case (1 2 3) (((cons $h $t) $h)))");

    mt_atom *e = E(1, 2, 3);
    mt_atom *tail = mt_expr_ref(mt_len(e) - 1, mt_children(e) + 1, mt_keep(e), drop_parent);
    assert(atom_is(mt_keep(mt_at(e, 0)), N(1)) && "C's head is the first child");
    assert(atom_is(tail, E(2, 3)) && "and the tail a view over the rest");
    mt_drop(e);
    mt_close(m);
    return 0;
}
