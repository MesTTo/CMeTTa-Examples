/* Purpose: list structure, twice over. In C an expression's head and tail
 *   need no copy: the tail is mt_expr_ref() over the parent's own child
 *   vector, holding the parent alive until the tail goes. len is a C function
 *   counting children, and the engine's own let over (cons $Head $Tail) and
 *   cons agree with the C view.
 * Guarantees: (1 2 3 4 5 6) splits as 1 and (2 3 4 5 6) both ways, (len (1 2
 *   3)) is 3, and (cons 42 ()) is (42) [tested: make twins; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

static void drop_parent(void *parent) { mt_drop(parent); }

/* The tail of a list as a view over its parent's children: no child is
   copied, and the parent lives as long as the view does. */
static mt_atom *tail_of(const mt_atom *list)
{
    mt_atom *parent = mt_keep(list);
    mt_atom *tail = mt_expr_ref(mt_len(list) - 1, mt_children(list) + 1, parent, drop_parent);
    if (!tail) mt_drop(parent);         /* a failed borrow leaves the owner here */
    return tail;
}

static mt_status len(mt_call *call, void *user)
{
    (void)user;
    const mt_atom *list = mt_arg(call, 0);
    if (mt_kind_of(list) != MT_EXPR) return MT_FAIL;
    return mt_answer(call, N((int64_t)mt_len(list)));
}

int main(void)
{
    metta *m = open_engine();
    mt_atom *six = E(1, 2, 3, 4, 5, 6);
    check_int("the head is 1", mt_int(mt_at(six, 0)), 1);
    check_atom("the tail is (2 3 4 5 6), sharing the parent's children",
               tail_of(six), E(2, 3, 4, 5, 6));
    /* (let (cons $Head $Tail) (1 2 3 4 5 6) ($Head $Tail)) */
    check_answers("the engine's cons pattern splits it the same way",
                  mt_eval(m, E("let", E("cons", V("Head"), V("Tail")), six, E(V("Head"), V("Tail")))),
                  E(1, E(2, 3, 4, 5, 6)));

    require("publish len", mt_def(m, (mt_op){ .name = "len", .arity = 1, .effect = MT_PURE, .fn = len }));
    check_int("(len (1 2 3)) is 3", mt_one_int(mt_eval(m, E("len", E(1, 2, 3)))), 3);
    check_answers("(cons 42 ()) is (42)", mt_eval(m, E("cons", 42, mt_unit())), E(42));
    return done(m);
}
