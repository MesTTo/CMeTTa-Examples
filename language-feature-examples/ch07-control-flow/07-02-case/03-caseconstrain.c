/* Purpose: destructuring an expression. The engine's case binds the head of
 *   (1 2 3) through a cons pattern; C holds the same expression as an array
 *   of children, so its head is mt_at(e, 0) and the rest a view over the
 *   same children, which mt_expr_ref builds without copying them.
 * Guarantees: the original's claim holds, and C's destructuring agrees
 *   [tested: make twins; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

static void drop_parent(void *parent) { mt_drop(parent); }

int main(void)
{
    metta *m = open_engine();
    check_answers("(case (1 2 3) (((cons $h $t) $h)))",
                  mt_eval(m, E("case", E(1, 2, 3), E(E(E("cons", V("h"), V("t")), V("h"))))), 1);

    mt_atom *e = E(1, 2, 3);
    mt_atom *tail = mt_expr_ref(mt_len(e) - 1, mt_children(e) + 1, mt_keep(e), drop_parent);
    check_atom("C's head is the first child", mt_keep(mt_at(e, 0)), N(1));
    check_atom("and the tail a view over the rest", tail, E(2, 3));
    mt_drop(e);
    return done(m);
}
