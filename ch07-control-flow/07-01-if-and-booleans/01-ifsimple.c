/* Purpose: if with no else. C has the same statement: keep() answers its
 *   value when the condition holds and returns MT_FAIL otherwise, which is
 *   the missing branch, and the engine's two-argument if agrees with it on
 *   both conditions.
 * Guarantees: the original's claim holds, with the false condition beside it
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>

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

/* (keep condition value): C's if without an else. */
static mt_status keep(mt_call *call, void *user)
{
    (void)user;
    if (mt_truth(mt_arg(call, 0))) return mt_answer(call, mt_keep(mt_arg(call, 1)));
    return MT_FAIL;
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("publish keep", mt_def(m, (mt_op){ .name = "keep", .arity = 2, .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = keep }));
    assert(answers_are(mt_eval(m, E("if", B(true), 42)), E(42)) && "(if True 42)");
    assert(answers_are(mt_eval(m, E("keep", B(true), 42)), E(42)) && "C's if answers the same");
    assert(!mt_first(mt_eval(m, E("if", B(false), 42))) && mt_ok() && "with the condition false, no answer");
    assert(!mt_first(mt_eval(m, E("keep", B(false), 42))) && mt_ok() && "from C either");
    mt_close(m);
    return 0;
}
