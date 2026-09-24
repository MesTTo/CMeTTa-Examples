/* Purpose: a C function refuses with words. mt_fail() turns a bad argument
 *   into an engine error whose message reaches the caller as a status, and
 *   the next request after a refusal is served normally.
 * Guarantees: the refusal's reason crosses back, and the function still
 *   answers afterwards [tested: make check; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

static mt_status positive(mt_call *call, void *user)
{
    (void)user;
    mt_clear();
    int64_t value = mt_int(mt_arg(call, 0));
    if (!mt_ok()) return mt_fail(call, "positive expects an integer");
    if (value < 0) return mt_fail(call, "positive refuses negative input");
    return mt_answer(call, N(value));
}

int main(void)
{
    metta *m = open_engine();
    require("publish positive", mt_def(m, (mt_op){ .name = "positive", .arity = 1,
                                                   .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = positive }));
    mt_clear();
    mt_atom *refused = mt_first(mt_eval(m, E("positive", -1)));
    check("the refusal is an error status", refused == NULL && mt_error() == MT_ERROR);
    check("carrying the function's own words",
          mt_errmsg() && strstr(mt_errmsg(), "negative input") != NULL);
    mt_clear();
    check_int("the next request is served", mt_one_int(mt_eval(m, E("positive", 42))), 42);
    require("withdraw positive", mt_undef(m, "positive"));
    return done(m);
}
