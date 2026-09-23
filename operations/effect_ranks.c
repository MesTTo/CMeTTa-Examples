/* Purpose: a C function declares what it does. Five copies of one function
 *   are published under the five effect classes, and the engine's effect
 *   plan, read without running anything, reports each class as declared.
 * Guarantees: every class survives registration into the plan [tested: make
 *   check; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

static mt_status same(mt_call *call, void *user)
{
    (void)user;
    return mt_answer(call, mt_keep(mt_arg(call, 0)));
}

int main(void)
{
    metta *m = open_engine();
    static const struct { const char *name; mt_effect effect; } ranks[] = {
        { "pure-id", MT_PURE }, { "read-id", MT_LOOKUP }, { "many-id", MT_NONDET },
        { "write-id", MT_WRITES }, { "io-id", MT_IO },
    };
    for (size_t i = 0; i < 5; i++) {
        require("publish", mt_def(m, (mt_op){ .name = ranks[i].name, .arity = 1,
                                              .effect = ranks[i].effect, .fn = same }));
        check_answers("it answers", mt_eval(m, E(ranks[i].name, 42)), 42);
        mt_atom *plan = mt_effect_plan(m, E(ranks[i].name, 42));
        require("plan it", plan != NULL);
        check("the plan reports the declared class",
              mt_alpha_eq(mt_at(plan, 1), S(mt_effect_str(ranks[i].effect))) &&
              mt_len(mt_at(plan, 2)) == 1);
        mt_drop(plan);
        require("withdraw", mt_undef(m, ranks[i].name));
    }
    return done(m);
}
