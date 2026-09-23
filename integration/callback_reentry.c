/* Purpose: Call MeTTa again from the C callback invoked by MeTTa.
 * Owns resources: local C handles are released before exit; a failed check
 *   terminates the example process.
 * Guarantees: the assertions below hold [tested: make check; commit=6022c3f48b6dc64752c6e49cfe9d985c7ac7a4e9].
 * Open Obligations: None.
 */
#include "common.h"
static mt_status delegate(mt_call *call, void *user)
{
    (void)user;
    mt_atom *value = mt_one(mt_eval(mt_of(call),
        mt_expr("double", mt_keep(mt_arg(call, 0)))));
    if (!value) return mt_error();
    return mt_answer(call, value); /* Transfers the nested answer. */
}
int main(void)
{
    metta *m = open_engine();
    check("install equation", mt_do(m, "(= (double $x) (* 2 $x))"));
    check("publish callback", mt_def(m, (mt_op){.name="delegate", .arity=1,
          .effect=MT_PURE, .fn=delegate}));
    check_answers("C to MeTTa to C to MeTTa", mt_run(m, "!(delegate 21)"), "42");
    check("withdraw callback", mt_undef(m, "delegate"));
    return done(m, "callback_reentry");
}
