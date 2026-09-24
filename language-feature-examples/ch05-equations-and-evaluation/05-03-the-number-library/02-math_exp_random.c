/* Purpose: exp-math is libm's exp, answer for answer, and log-math inverts
 *   it within a float's error, which C measures with fabs; the dice draw
 *   inside their bounds, which in-range, a C function comparing with
 *   mt_compare, checks inside the engine on every draw.
 * Guarantees: all seven claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include <math.h>

/* (in-range lo hi x): lo <= x <= hi in the standard order, which for numbers
   of one kind is their value. */
static mt_status in_range(mt_call *call, void *user)
{
    (void)user;
    const mt_atom *lo = mt_arg(call, 0), *hi = mt_arg(call, 1), *x = mt_arg(call, 2);
    return mt_answer(call, B(mt_compare(lo, x) <= 0 && mt_compare(x, hi) <= 0));
}

int main(void)
{
    metta *m = open_engine();
    const double e = exp(1.0);

    check_answers("(exp-math 0)", mt_eval(m, E("exp-math", 0)), exp(0.0));
    check_answers("(exp-math 1.0) is e", mt_eval(m, E("exp-math", 1.0)), e);
    check("e squared, within 1e-12", fabs(mt_one_float(mt_eval(m, E("exp-math", 2.0))) - e * e) < 1.0e-12);
    check("log base e undoes exp-math, within 1e-12",
          fabs(mt_one_float(mt_eval(m, E("log-math", e, E("exp-math", 3.0)))) - 3.0) < 1.0e-12);

    require("publish in-range", mt_def(m, (mt_op){ .name = "in-range", .arity = 3,
                                                 .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = in_range }));
    check_answers("a die lands in 1..6", mt_eval(m, E("in-range", 1, 6, E("random-int", 1, 6))), B(true));
    check_answers("a float draw lands in [0, 1]",
                  mt_eval(m, E("in-range", 0.0, 1.0, E("random-float", 0.0, 1.0))), B(true));
    check_answers("a one-sided die lands on 5", mt_eval(m, E("in-range", 5, 5, E("random-int", 5, 5))), B(true));
    return done(m);
}
