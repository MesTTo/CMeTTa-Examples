/* Purpose: a C function declares what it does. One copy of one function is
 *   published under each effect class, however many the engine's
 *   effect-class vocabulary holds, each named from its class's word, and the
 *   engine's effect plan, read without running anything, reports each class
 *   as declared.
 * Guarantees: every class survives registration into the plan
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A door the program needs before it can go on: on refusal, say why and stop. */
#define require(what, ok) \
    ((ok) ? (void)0 : (fprintf(stderr, "%s: %s\n", (what), mt_errmsg()), exit(EXIT_FAILURE)))

/* Whether a list holds exactly the children of want, in order, each equal
   up to renaming variables; takes both, and shows them when they differ. */
static inline bool list_is(mt_list got, mt_atom *want)
{
    bool holds = mt_ok() && want && got.len == mt_len(want);
    for (size_t i = 0; holds && i < got.len; i++)
        holds = mt_alpha_eq(got.items[i], mt_at(want, i));
    if (!holds) {
        fprintf(stderr, "  got");
        for (size_t i = 0; i < got.len; i++) fprintf(stderr, " %s", mt_show(got.items[i]));
        fprintf(stderr, "\n  want %s\n", want ? mt_show(want) : "nothing");
    }
    mt_list_free(got);
    mt_drop(want);
    return holds;
}

/* Whether a query answers exactly the children of want; takes both. */
static inline bool answers_are(mt_answers *answers, mt_atom *want)
{
    return list_is(mt_all(answers), want);
}

static mt_status same(mt_call *call, void *user)
{
    (void)user;
    return mt_answer(call, mt_keep(mt_arg(call, 0)));
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    for (size_t i = 0; i < MT_VOCABULARY_COUNT(mt_effect_class_names); i++) {
        const enum mt_effect_class effect = (enum mt_effect_class)i;
        char name[64];
        snprintf(name, sizeof name, "id-%s", mt_effect_class_names[effect]);
        require("publish", mt_def(m, (mt_op){ .name = name, .arity = 1, .effect = effect, .fn = same }));
        assert(answers_are(mt_eval(m, E(name, 42)), E(42)) && "it answers");
        mt_atom *plan = mt_effect_plan(m, E(name, 42));
        require("plan it", plan != NULL);
        const mt_atom *declared = mt_at(plan, 1);
        assert(mt_kind_of(declared) == MT_SYMBOL &&
strcmp(mt_name(declared), mt_effect_class_names[effect]) == 0 && mt_len(mt_at(plan, 2)) == 1
               && "the plan reports the declared class");
        mt_drop(plan);
        require("withdraw", mt_undef(m, name));
    }
    mt_close(m);
    return 0;
}
