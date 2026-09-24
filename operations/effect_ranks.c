/* Purpose: a C function declares what it does. One copy of one function is
 *   published under each effect class, however many the engine's
 *   effect-class vocabulary holds, each named from its class's word, and the
 *   engine's effect plan, read without running anything, reports each class
 *   as declared.
 * Guarantees: every class survives registration into the plan [tested: make
 *   check; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
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
    for (size_t i = 0; i < MT_VOCABULARY_COUNT(mt_effect_class_names); i++) {
        const enum mt_effect_class effect = (enum mt_effect_class)i;
        char name[64];
        snprintf(name, sizeof name, "id-%s", mt_effect_class_names[effect]);
        require("publish", mt_def(m, (mt_op){ .name = name, .arity = 1, .effect = effect, .fn = same }));
        check_answers("it answers", mt_eval(m, E(name, 42)), 42);
        mt_atom *plan = mt_effect_plan(m, E(name, 42));
        require("plan it", plan != NULL);
        const mt_atom *declared = mt_at(plan, 1);
        check("the plan reports the declared class", mt_kind_of(declared) == MT_SYMBOL &&
              strcmp(mt_name(declared), mt_effect_class_names[effect]) == 0 && mt_len(mt_at(plan, 2)) == 1);
        mt_drop(plan);
        require("withdraw", mt_undef(m, name));
    }
    return done(m);
}
