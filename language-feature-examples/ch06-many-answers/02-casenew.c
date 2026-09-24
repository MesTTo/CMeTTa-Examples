/* Purpose: a branch that answers nothing. wu1 and wu2 are C functions: wu1
 *   returns MT_FAIL, which is C saying it has no answer for these arguments
 *   and is what (empty) is, and wu2 answers (full); wu superposes the two
 *   calls, and only wu2's answer is left.
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

static mt_status full(mt_call *call, void *user)
{
    (void)user;
    return mt_answer(call, E("full"));
}

int main(void)
{
    metta *m = open_engine();
    require("publish wu1", mt_def(m, (mt_op){ .name = "wu1", .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = nothing }));
    require("publish wu2", mt_def(m, (mt_op){ .name = "wu2", .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = full }));
    require("(= (wu) (superpose ((wu1) (wu2))))",
            mt_add(m, E("=", E("wu"), E("superpose", E(E("wu1"), E("wu2"))))));
    check_answers("the empty branch drops out", mt_eval(m, E("wu")), E("full"));
    return done(m);
}
