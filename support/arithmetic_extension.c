/* Purpose: export a CMeTTa plugin entry point from an independently linked DSO.
 * Owns resources: the runtime retains the DSO while its registered code is live.
 * Guarantees: shared_extension checks the published operation [tested: make check; commit=WORKTREE].
 * Open Obligations: None.
 */
#include <cmetta.h>
static mt_status triple(mt_call *call, void *user)
{
    (void)user;
    return mt_answer(call, mt_one(mt_eval(mt_of(call), mt_expr("*", 3, mt_keep(mt_arg(call, 0))))));
}
bool mt_extension_init(metta *m)
{ return mt_def(m, (mt_op){.name="plugin-triple", .arity=1, .effect=MT_PURE, .fn=triple}); }
