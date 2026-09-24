/* Purpose: exp is libm's exp under the corelib's name and one operation with
 *   exp-math; with-seed is srand for one body. seeded() evaluates a draw
 *   under a seed and keeps the value, so two runs of one seed are compared
 *   the way C compares two runs after srand: the same seed, the same
 *   sequence, a different seed, a different one, and random-float drawn from
 *   the same generator.
 * Guarantees: all seven claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include <math.h>

/* The body's answer with the generator seeded, (with-seed seed body). */
static mt_atom *seeded(metta *m, int64_t seed, mt_atom *body)
{
    return mt_one(mt_eval(m, E("with-seed", seed, body)));
}

static mt_atom *draw(void) { return E("random-int", 0, 100); }

/* Two draws inside one seeding, collapsed, so the whole sequence is one value. */
static mt_atom *two_draws(void) { return E("collapse", E("superpose", E(draw(), draw()))); }

int main(void)
{
    metta *m = open_engine();

    check_answers("(exp 0)", mt_eval(m, E("exp", 0)), exp(0.0));
    check_answers("(exp 1)", mt_eval(m, E("exp", 1)), exp(1.0));
    check_atom("exp and exp-math are one operation",
               mt_one(mt_eval(m, E("exp", 2))), mt_one(mt_eval(m, E("exp-math", 2))));

    check_atom("one seed draws one number", seeded(m, 42, draw()), seeded(m, 42, draw()));
    check_atom("and one sequence", seeded(m, 7, two_draws()), seeded(m, 7, two_draws()));
    mt_atom *at42 = seeded(m, 42, draw()), *at43 = seeded(m, 43, draw());
    check("a different seed draws differently", at42 && at43 && !mt_eq(at42, at43));
    mt_drop(at42);
    mt_drop(at43);
    check_atom("random-float shares the generator",
               seeded(m, 5, E("random-float", 0.0, 1.0)), seeded(m, 5, E("random-float", 0.0, 1.0)));
    return done(m);
}
