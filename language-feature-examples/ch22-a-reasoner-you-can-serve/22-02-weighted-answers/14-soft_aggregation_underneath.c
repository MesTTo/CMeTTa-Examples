/* Purpose: the three operations under lib_soft's scorer, held to C's model
 *   of them in soft.h: the test for a written symbol, the fold dispatched on
 *   its aggregation's name, and the walk each fold runs with its accumulator
 *   exposed. The similarity facts are C's table, added as the atoms they
 *   are. Where the original compares the mean fold with its walk divided by
 *   the count, or the declared score with the mean fold, C compares the
 *   engine's two answers.
 * Guarantees: all twenty claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"
#include "soft.h"

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
    metta *m = open_engine();
    require("lib_soft", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_soft")))));
    for (size_t i = 0; i < DECLARED; i++) require("a similarity", mt_add(m, E("similar", declared[i].a, declared[i].b, declared[i].degree)));
    soft model = { declared, DECLARED, SOFT_MIN };

    mt_atom *written[] = { S("cat"), S("min"), N(1), T("text"), E("f", 1) };
    for (size_t i = 0; i < sizeof written / sizeof *written; i++) {
        check_answers("soft-symbol?", mt_eval(m, E("soft-symbol?", mt_keep(written[i]))), B(soft_symbol(written[i])));
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
        check_answers("soft-fold", mt_eval(m, E("soft-fold", aggregation_names[folds[i].agg], E(folds[i].p[0], folds[i].p[1]), E(folds[i].a[0], folds[i].a[1]))),
                      folded(&model, folds[i].agg, false, 0, E(folds[i].p[0], folds[i].p[1]), E(folds[i].a[0], folds[i].a[1])));

    /* The walks, the accumulator given: min's minima, mean's sum. */
    check_answers("the min walk", mt_eval(m, E("soft-walk", "min", E("cat", "fish"), E("feline", "shark"), 1.0)),
                  folded(&model, SOFT_MIN, true, 1.0, E("cat", "fish"), E("feline", "shark")));
    check_answers("the mean walk sums", mt_eval(m, E("soft-walk", "mean", E("cat", "fish"), E("feline", "shark"), 0.0)),
                  folded(&model, SOFT_MEAN, true, 0.0, E("cat", "fish"), E("feline", "shark")));
    check_atom("the mean is the walk over the count", mt_one(mt_eval(m, E("soft-fold", "mean", E("cat", "fish"), E("feline", "shark")))),
               mt_one(mt_eval(m, E("/", E("soft-walk", "mean", E("cat", "fish"), E("feline", "shark"), 0.0), 2))));

    /* min stops at zero, and from zero it never starts. */
    check_answers("min stops at the first stranger", mt_eval(m, E("soft-walk", "min", E("dog", "whale", "otter"), E("cat", "shark", "seal"), 1.0)),
                  folded(&model, SOFT_MIN, true, 1.0, E("dog", "whale", "otter"), E("cat", "shark", "seal")));
    check_answers("a zero accumulator stops at once", mt_eval(m, E("soft-walk", "min", E("dog", "whale"), E("cat", "shark"), 0.0)),
                  folded(&model, SOFT_MIN, true, 0.0, E("dog", "whale"), E("cat", "shark")));
    check_answers("min over nothing", mt_eval(m, E("soft-walk", "min", mt_unit(), mt_unit(), 1.0)),
                  folded(&model, SOFT_MIN, true, 1.0, mt_unit(), mt_unit()));
    check_answers("mean over nothing", mt_eval(m, E("soft-walk", "mean", mt_unit(), mt_unit(), 0.0)),
                  folded(&model, SOFT_MEAN, true, 0.0, mt_unit(), mt_unit()));

    /* soft-score folds with the space's declaration. */
    check_answers("undeclared, min", mt_eval(m, E("soft-score", E("likes", "cat", "fish"), E("likes", "feline", "shark"))),
                  folded(&model, model.declared, false, 0, E("likes", "cat", "fish"), E("likes", "feline", "shark")));
    require("declare mean", mt_add(m, E("soft-aggregate", aggregation_names[SOFT_MEAN])));
    model.declared = SOFT_MEAN;
    check_atom("declared, the mean fold", mt_one(mt_eval(m, E("soft-score", E("likes", "cat", "fish"), E("likes", "feline", "shark")))),
               mt_one(mt_eval(m, E("soft-fold", aggregation_names[model.declared], E("likes", "cat", "fish"), E("likes", "feline", "shark")))));
    return done(m);
}
