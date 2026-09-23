/* Purpose: the Empty branch is the one a key with no answers takes. wu's key
 *   is (empty), so it takes Empty; wu2's key is (f), a C function answering
 *   42, so it takes the 42 branch instead.
 * Guarantees: both claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

static mt_status f(mt_call *call, void *user)
{
    (void)user;
    return mt_answer(call, N(42));
}

int main(void)
{
    metta *m = open_engine();
    require("publish f", mt_def(m, (mt_op){ .name = "f", .effect = MT_PURE, .fn = f }));
    require("wu", mt_lower(m, (wu), (case (empty) ((1 2) (Empty 42)))));
    require("wu2", mt_lower(m, (wu2), (case (f) ((42 ok) (Empty nok)))));
    check_answers("a key with no answers takes Empty", mt_eval(m, E("wu")), 42);
    check_answers("a key that answers takes its own branch", mt_eval(m, E("wu2")), "ok");
    return done(m);
}
