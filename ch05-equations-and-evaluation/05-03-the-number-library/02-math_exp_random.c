/* Purpose: exp-math is libm's exp, answer for answer, and log-math inverts
 *   it within a float's error, which C measures with fabs; the dice draw
 *   inside their bounds, which in-range, a C function comparing with
 *   mt_compare, checks inside the engine on every draw.
 * Guarantees: all seven claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

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
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    const double e = exp(1.0);

    assert(answers_are(mt_eval(m, E("exp-math", 0)), E(exp(0.0))) && "(exp-math 0)");
    assert(answers_are(mt_eval(m, E("exp-math", 1.0)), E(e)) && "(exp-math 1.0) is e");
    assert(fabs(mt_one_float(mt_eval(m, E("exp-math", 2.0))) - e * e) < 1.0e-12 && "e squared, within 1e-12");
    assert(fabs(mt_one_float(mt_eval(m, E("log-math", e, E("exp-math", 3.0)))) - 3.0) < 1.0e-12
           && "log base e undoes exp-math, within 1e-12");

    require("publish in-range", mt_def(m, (mt_op){ .name = "in-range", .arity = 3,
                                                 .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = in_range }));
    assert(answers_are(mt_eval(m, E("in-range", 1, 6, E("random-int", 1, 6))), E(B(true))) && "a die lands in 1..6");
    assert(answers_are(mt_eval(m, E("in-range", 0.0, 1.0, E("random-float", 0.0, 1.0))), E(B(true)))
           && "a float draw lands in [0, 1]");
    assert(answers_are(mt_eval(m, E("in-range", 5, 5, E("random-int", 5, 5))), E(B(true))) && "a one-sided die lands on 5");
    mt_close(m);
    return 0;
}
