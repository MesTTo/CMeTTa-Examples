/* Purpose: a C function the engine calls may call the engine. delegate asks
 *   MeTTa for (double x) while MeTTa is inside delegate, and hands the nested
 *   answer back as its own.
 * Guarantees: (delegate 21) is 42 through C to MeTTa to C to MeTTa
 *   [tested: make check; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

static mt_status delegate(mt_call *call, void *user)
{
    (void)user;
    mt_atom *doubled = mt_one(mt_eval(mt_of(call), E("double", mt_keep(mt_arg(call, 0)))));
    if (!doubled) return mt_error();
    return mt_answer(call, doubled);        /* the nested answer is taken */
}

int main(void)
{
    metta *m = open_engine();
    /* (= (double $x) (* 2 $x)) */
    require("define double", mt_add(m, E("=", E("double", V("x")), E("*", 2, V("x")))));
    require("publish delegate", mt_def(m, (mt_op){ .name = "delegate", .arity = 1,
                                                   .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = delegate }));
    check_int("C calls MeTTa from inside a call MeTTa made",
              mt_one_int(mt_eval(m, E("delegate", 21))), 42);
    require("withdraw delegate", mt_undef(m, "delegate"));
    return done(m);
}
