/* Purpose: equations move, one at a time. f has two equations, one calling
 *   its argument and one answering 42; each is an atom C removes and puts
 *   back, and the answers follow. g, the function f is handed, is C.
 * Guarantees: (f g) answers 2 and 42, then 2, then 42, then itself [tested:
 *   make twins; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

static mt_status plus_one(mt_call *call, void *user)
{
    (void)user;
    mt_clear();
    int64_t x = mt_int(mt_arg(call, 0));
    if (!mt_ok()) return mt_fail(call, "g adds one to an integer");
    return mt_answer(call, N(x + 1));
}

int main(void)
{
    metta *m = open_engine();
    require("publish g", mt_def(m, (mt_op){ .name = "g", .arity = 1, .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = plus_one }));
    mt_atom *calls = E("=", E("f", V("g")), E(V("g"), 1));      /* (= (f $g) ($g 1)) */
    mt_atom *constant = E("=", E("f", V("g")), 42);              /* (= (f $g) 42) */
    require("the calling equation", mt_add(m, mt_keep(calls)));
    require("the constant one", mt_add(m, mt_keep(constant)));
    check_answers("both equations answer", mt_eval(m, E("f", "g")), 2, 42);

    require("take the constant out", mt_del(m, mt_keep(constant)));
    check_answers("only the call is left", mt_eval(m, E("f", "g")), 2);

    require("put the constant back", mt_add(m, mt_keep(constant)));
    require("take the call out", mt_del(m, calls));
    check_answers("only the constant is left", mt_eval(m, E("f", "g")), 42);

    require("take the constant out again", mt_del(m, constant));
    check_answers("with no equation the call answers itself", mt_eval(m, E("f", "g")), E("f", "g"));
    return done(m);
}
