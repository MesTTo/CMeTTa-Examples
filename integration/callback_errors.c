/* Purpose: Return explicit callback errors and recover for the next request.
 * Owns resources: local C handles are released before exit; a failed check
 *   terminates the example process.
 * Guarantees: the assertions below hold [tested: make check; commit=WORKTREE].
 * Open Obligations: None.
 */
#include "common.h"
static mt_status positive(mt_call *call, void *user)
{
    (void)user;
    mt_clear();
    int64_t value = mt_int(mt_arg(call, 0));
    if (!mt_ok()) return mt_fail(call, "positive expects an integer");
    if (value < 0) return mt_fail(call, "positive refuses negative input");
    return mt_answer(call, mt_num(value));
}
int main(void)
{
    metta *m = open_engine();
    check("publish positive", mt_def(m, (mt_op){.name="positive", .arity=1, .effect=MT_PURE, .fn=positive}));
    mt_answers *bad = mt_run(m, "!(positive -1)");
    check("reason crosses FFI", !bad && mt_error() == MT_ERROR &&
          strstr(mt_errmsg(), "negative input") != NULL);
    mt_clear();
    check_answers("later request succeeds", mt_run(m, "!(positive 42)"), "42");
    check("withdraw operation", mt_undef(m, "positive"));
    return done(m, "callback_errors");
}

