/* Purpose: a branch that answers nothing. wu1 and wu2 are C functions: wu1
 *   returns MT_FAIL, which is C saying it has no answer for these arguments
 *   and is what (empty) is, and wu2 answers (full); wu superposes the two
 *   calls, and only wu2's answer is left.
 * Guarantees: the original's claim holds [tested 2026-09-27T00:35:58+10:00:
 *   make -C extensions/cmetta corpus-check].
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

static mt_status nothing(mt_call *call, void *user)
{
    (void)call;
    (void)user;
    return MT_FAIL;
}

static mt_status full(mt_call *call, void *user)
{
    (void)user;
    return mt_answer(call, E("full"));
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("publish wu1", mt_def(m, (mt_op){ .name = "wu1", .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = nothing }));
    require("publish wu2", mt_def(m, (mt_op){ .name = "wu2", .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = full }));
    require("(= (wu) (superpose ((wu1) (wu2))))",
            mt_add(m, E("=", E("wu"), E("superpose", E(E("wu1"), E("wu2"))))));
    assert(answers_are(mt_eval(m, E("wu")), E(E("full"))) && "the empty branch drops out");
    mt_close(m);
    return 0;
}
