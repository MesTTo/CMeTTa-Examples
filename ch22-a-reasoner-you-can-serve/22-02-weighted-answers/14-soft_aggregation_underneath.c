/* Purpose: the three operations under lib_soft's scorer, held to C's model
 *   of them in soft.h: the test for a written symbol, the fold dispatched on
 *   its aggregation's name, and the walk each fold runs with its accumulator
 *   exposed. The similarity facts are C's table, added as the atoms they
 *   are. Where the original compares the mean fold with its walk divided by
 *   the count, or the declared score with the mean fold, C compares the
 *   engine's two answers.
 * Guarantees: all twenty claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include "_fixtures/soft.h"

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

static const similarity declared[] = { { "cat", "feline", 0.8 }, { "fish", "shark", 0.5 } };
#define DECLARED (sizeof declared / sizeof *declared)

/* C's fold or walk of P against A, which must answer. TAKES both. */
static double folded(const soft *s, aggregation agg, bool walk, double acc, mt_atom *p, mt_atom *a)
{
    soft_bindings bound = { NULL, 0 };
    double out = 0;
    require("C's model answers", walk ? soft_walk(s, agg, p, a, acc, &bound, &out) : soft_fold(s, agg, p, a, &bound, &out));
    soft_bindings_free(&bound), mt_drop(p), mt_drop(a);
    return out;
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("lib_soft", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_soft")))));
    for (size_t i = 0; i < DECLARED; i++) require("a similarity", mt_add(m, E("similar", declared[i].a, declared[i].b, declared[i].degree)));
    soft model = { declared, DECLARED, SOFT_MIN };

    mt_atom *written[] = { S("cat"), S("min"), N(1), T("text"), E("f", 1) };
    for (size_t i = 0; i < sizeof written / sizeof *written; i++) {
        assert(answers_are(mt_eval(m, E("is-symbol", mt_keep(written[i]))), E(B(soft_symbol(written[i])))) && "is-symbol");
        mt_drop(written[i]);
    }

    static const struct {
        aggregation agg;
        const char *p[2], *a[2];
    } folds[] = {
        { SOFT_MIN, { "cat", "fish" }, { "feline", "fish" } },  { SOFT_MIN, { "cat", "fish" }, { "feline", "shark" } },
        { SOFT_MIN, { "cat", "fish" }, { "dog", "fish" } },     { SOFT_MEAN, { "cat", "fish" }, { "feline", "shark" } },
        { SOFT_MEAN, { "cat", "fish" }, { "dog", "fish" } },    { SOFT_MIN, { "cat", "fish" }, { "dog", "fish" } },
    };
    for (size_t i = 0; i < sizeof folds / sizeof *folds; i++)
        assert(answers_are(mt_eval(m, E("soft-fold", aggregation_names[folds[i].agg], E(folds[i].p[0], folds[i].p[1]), E(folds[i].a[0], folds[i].a[1]))), E(folded(&model, folds[i].agg, false, 0, E(folds[i].p[0], folds[i].p[1]), E(folds[i].a[0], folds[i].a[1]))))
               && "soft-fold");

    /* The walks, the accumulator given: min's minima, mean's sum. */
    assert(answers_are(mt_eval(m, E("soft-walk", "min", E("cat", "fish"), E("feline", "shark"), 1.0)), E(folded(&model, SOFT_MIN, true, 1.0, E("cat", "fish"), E("feline", "shark"))))
           && "the min walk");
    assert(answers_are(mt_eval(m, E("soft-walk", "mean", E("cat", "fish"), E("feline", "shark"), 0.0)), E(folded(&model, SOFT_MEAN, true, 0.0, E("cat", "fish"), E("feline", "shark"))))
           && "the mean walk sums");
    assert(atom_is(mt_one(mt_eval(m, E("soft-fold", "mean", E("cat", "fish"), E("feline", "shark")))), mt_one(mt_eval(m, E("/", E("soft-walk", "mean", E("cat", "fish"), E("feline", "shark"), 0.0), 2))))
           && "the mean is the walk over the count");

    /* min stops at zero, and from zero it never starts. */
    assert(answers_are(mt_eval(m, E("soft-walk", "min", E("dog", "whale", "otter"), E("cat", "shark", "seal"), 1.0)), E(folded(&model, SOFT_MIN, true, 1.0, E("dog", "whale", "otter"), E("cat", "shark", "seal"))))
           && "min stops at the first stranger");
    assert(answers_are(mt_eval(m, E("soft-walk", "min", E("dog", "whale"), E("cat", "shark"), 0.0)), E(folded(&model, SOFT_MIN, true, 0.0, E("dog", "whale"), E("cat", "shark"))))
           && "a zero accumulator stops at once");
    assert(answers_are(mt_eval(m, E("soft-walk", "min", mt_unit(), mt_unit(), 1.0)), E(folded(&model, SOFT_MIN, true, 1.0, mt_unit(), mt_unit())))
           && "min over nothing");
    assert(answers_are(mt_eval(m, E("soft-walk", "mean", mt_unit(), mt_unit(), 0.0)), E(folded(&model, SOFT_MEAN, true, 0.0, mt_unit(), mt_unit())))
           && "mean over nothing");

    /* soft-score folds with the space's declaration. */
    assert(answers_are(mt_eval(m, E("soft-score", E("likes", "cat", "fish"), E("likes", "feline", "shark"))), E(folded(&model, model.declared, false, 0, E("likes", "cat", "fish"), E("likes", "feline", "shark"))))
           && "undeclared, min");
    require("declare mean", mt_add(m, E("soft-aggregate", aggregation_names[SOFT_MEAN])));
    model.declared = SOFT_MEAN;
    assert(atom_is(mt_one(mt_eval(m, E("soft-score", E("likes", "cat", "fish"), E("likes", "feline", "shark")))), mt_one(mt_eval(m, E("soft-fold", aggregation_names[model.declared], E("likes", "cat", "fish"), E("likes", "feline", "shark")))))
           && "declared, the mean fold");
    mt_close(m);
    return 0;
}
