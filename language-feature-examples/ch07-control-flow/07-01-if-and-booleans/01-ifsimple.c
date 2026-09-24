/* Purpose: if with no else. C has the same statement: keep() answers its
 *   value when the condition holds and returns MT_FAIL otherwise, which is
 *   the missing branch, and the engine's two-argument if agrees with it on
 *   both conditions.
 * Guarantees: the original's claim holds, with the false condition beside it
 *   [tested: make twins; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

/* (keep condition value): C's if without an else. */
static mt_status keep(mt_call *call, void *user)
{
    (void)user;
    if (mt_truth(mt_arg(call, 0))) return mt_answer(call, mt_keep(mt_arg(call, 1)));
    return MT_FAIL;
}

int main(void)
{
    metta *m = open_engine();
    require("publish keep", mt_def(m, (mt_op){ .name = "keep", .arity = 2, .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = keep }));
    check_answers("(if True 42)", mt_eval(m, E("if", B(true), 42)), 42);
    check_answers("C's if answers the same", mt_eval(m, E("keep", B(true), 42)), 42);
    check_none("with the condition false, no answer", mt_eval(m, E("if", B(false), 42)));
    check_none("from C either", mt_eval(m, E("keep", B(false), 42)));
    return done(m);
}
