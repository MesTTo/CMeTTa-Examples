/* Purpose: classify C callbacks explicitly and inspect the language effect rows.
 * Owns resources: registrations are withdrawn before their user data expires.
 * Guarantees: the engine reports each declared rank [tested: make check; commit=6022c3f48b6dc64752c6e49cfe9d985c7ac7a4e9].
 * Open Obligations: None.
 */
#include "common.h"
static mt_status identity(mt_call *call, void *user)
{ (void)user; return mt_answer(call, mt_keep(mt_arg(call, 0))); }
int main(void)
{
    metta *m = open_engine();
    const mt_effect effects[] = {MT_PURE, MT_LOOKUP, MT_NONDET, MT_WRITES, MT_IO};
    const char *names[] = {"pure-id", "read-id", "many-id", "write-id", "io-id"};
    for (size_t i = 0; i < sizeof(effects)/sizeof(effects[0]); ++i) {
        check("declare callback effect", mt_def(m, (mt_op){.name=names[i], .arity=1, .effect=effects[i], .fn=identity}));
        check_answers("callback result", mt_eval(m, mt_expr(names[i], 42)), "42");
        mt_atom *report = mt_effect_plan(m, mt_expr(names[i], 42));
        check("plan present", report != NULL);
        check_atom("declared effect survives registration", mt_at(report, 1), mt_effect_str(effects[i]));
        check("operation roster", mt_len(mt_at(report, 2)) == 1); mt_drop(report);
        check("withdraw callback", mt_undef(m, names[i]));
    }
    return done(m, "effect_ranks");
}
