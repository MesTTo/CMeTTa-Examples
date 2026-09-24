/* Purpose: a call before its callee exists. (= (f) (g)) is stored while g
 *   means nothing, and it is only a term until something reduces it; g then
 *   arrives as a C function, and both f and h, which call it, answer what C
 *   answers.
 * Guarantees: (f) and (h) are 42 [tested: make twins; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

static mt_status g(mt_call *call, void *user)
{
    (void)user;
    return mt_answer(call, N(42));
}

int main(void)
{
    metta *m = open_engine();
    require("(= (f) (g)), before g exists", mt_add(m, E("=", E("f"), E("g"))));
    require("publish g", mt_def(m, (mt_op){ .name = "g", .arity = 0, .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = g }));
    require("(= (h) (g))", mt_add(m, E("=", E("h"), E("g"))));
    check_int("(f) reaches the C function", mt_one_int(mt_eval(m, E("f"))), 42);
    check_int("and so does (h)", mt_one_int(mt_eval(m, E("h"))), 42);
    return done(m);
}
