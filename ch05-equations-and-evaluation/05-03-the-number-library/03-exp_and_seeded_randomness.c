/* Purpose: exp is libm's exp under the corelib's name and one operation with
 *   exp-math; with-seed is srand for one body. seeded() evaluates a draw
 *   under a seed and keeps the value, so two runs of one seed are compared
 *   the way C compares two runs after srand: the same seed, the same
 *   sequence, a different seed, a different one, and random-float drawn from
 *   the same generator.
 * Guarantees: all seven claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <math.h>

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

/* Whether an atom is want up to renaming variables; takes both, and shows
   them when they differ. */
static inline bool atom_is(mt_atom *got, mt_atom *want)
{
    bool holds = got && want && mt_alpha_eq(got, want);
    if (!holds) fprintf(stderr, "  got %s\n  want %s\n", got ? mt_show(got) : "nothing",
                        want ? mt_show(want) : "nothing");
    mt_drop(got);
    mt_drop(want);
    return holds;
}

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
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;

    assert(answers_are(mt_eval(m, E("exp", 0)), E(exp(0.0))) && "(exp 0)");
    assert(answers_are(mt_eval(m, E("exp", 1)), E(exp(1.0))) && "(exp 1)");
    assert(atom_is(mt_one(mt_eval(m, E("exp", 2))), mt_one(mt_eval(m, E("exp-math", 2))))
           && "exp and exp-math are one operation");

    assert(atom_is(seeded(m, 42, draw()), seeded(m, 42, draw())) && "one seed draws one number");
    assert(atom_is(seeded(m, 7, two_draws()), seeded(m, 7, two_draws())) && "and one sequence");
    mt_atom *at42 = seeded(m, 42, draw()), *at43 = seeded(m, 43, draw());
    assert(at42 && at43 && !mt_eq(at42, at43) && "a different seed draws differently");
    mt_drop(at42);
    mt_drop(at43);
    assert(atom_is(seeded(m, 5, E("random-float", 0.0, 1.0)), seeded(m, 5, E("random-float", 0.0, 1.0)))
           && "random-float shares the generator");
    mt_close(m);
    return 0;
}
