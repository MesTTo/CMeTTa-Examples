/* Purpose: a function that answers nothing. In C that is a function
 *   returning MT_FAIL: no answer for these arguments, which the engine reads
 *   as (empty), so the call's answer list is empty.
 * Guarantees: the original's claim holds [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

static mt_status nothing(mt_call *call, void *user)
{
    (void)call;
    (void)user;
    return MT_FAIL;
}

int main(void)
{
    metta *m = open_engine();
    require("publish y", mt_def(m, (mt_op){ .name = "y", .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = nothing }));
    check_none("(y) answers nothing", mt_eval(m, E("y")));
    return done(m);
}
