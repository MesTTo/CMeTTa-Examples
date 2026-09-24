/* Purpose: a builtin half applied. mymap maps a function over a list, and
 *   the half-applied builtin (== 1) must map exactly as (eq 1) does, eq
 *   being a C function; C maps its own comparison over the same array for
 *   the list both must answer.
 * Guarantees: the original's claim holds, and both agree with C's map
 *   [tested: make twins; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

static mt_status eq(mt_call *call, void *user)
{
    (void)user;
    return mt_answer(call, B(mt_eq(mt_arg(call, 0), mt_arg(call, 1))));
}

static const int64_t XS[] = { 1, 2, 3 };

int main(void)
{
    metta *m = open_engine();
    require("(= (mymap $f ()) ())", mt_add(m, E("=", E("mymap", V("f"), mt_unit()), mt_unit())));
    require("mymap's cons case", mt_lower(m, (mymap $f (cons $x $xs)),
                                          (let $head ($f $x) (let $rest (mymap $f $xs) (cons $head $rest)))));
    require("publish eq", mt_def(m, (mt_op){ .name = "eq", .arity = 2, .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = eq }));

    mt_atom *mapped[3];
    for (size_t i = 0; i < 3; i++) mapped[i] = B(XS[i] == 1);
    mt_atom *expected = mt_exprv(3, mapped);
    check_atom("(== 1) maps as (eq 1)", mt_one(mt_eval(m, E("mymap", E("==", 1), E(1, 2, 3)))),
               mt_one(mt_eval(m, E("mymap", E("eq", 1), E(1, 2, 3)))));
    check_answers("and both as C's comparison", mt_eval(m, E("mymap", E("eq", 1), E(1, 2, 3))), expected);
    return done(m);
}
