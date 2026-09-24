/* Purpose: an iterator whose state is a number. make-nat-iter and
 *   iter-next are C functions over that state, next answering the value and
 *   the state after it; the engine's let* threads the state through three
 *   steps, and C steps the same protocol in a loop for the values it
 *   expects.
 * Guarantees: the original's claim holds [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

static mt_status make_nat_iter(mt_call *call, void *user)
{
    (void)user;
    return mt_answer(call, N(0));
}

/* (iter-next n): (n n+1), the value and the next state. */
static mt_status iter_next(mt_call *call, void *user)
{
    (void)user;
    int64_t n = mt_int(mt_arg(call, 0));
    return mt_answer(call, E(n, n + 1));
}

int main(void)
{
    metta *m = open_engine();
    require("publish make-nat-iter", mt_def(m, (mt_op){ .name = "make-nat-iter", .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = make_nat_iter }));
    require("publish iter-next", mt_def(m, (mt_op){ .name = "iter-next", .arity = 1, .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = iter_next }));

    mt_atom *seen[3];
    int64_t state = 0;
    for (int i = 0; i < 3; i++) seen[i] = N(state++);
    check_answers("three steps of the iterator",
                  mt_eval(m, E("let*", E(E(V("it"), E("make-nat-iter")),
                                         E(E(V("x1"), V("it1")), E("iter-next", V("it"))),
                                         E(E(V("x2"), V("it2")), E("iter-next", V("it1"))),
                                         E(E(V("x3"), V("it3")), E("iter-next", V("it2")))),
                               E(V("x1"), V("x2"), V("x3")))),
                  mt_exprv(3, seen));
    return done(m);
}
