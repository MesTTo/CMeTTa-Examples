/* Purpose: soft unification over a space, held to C's model of lib_soft in
 *   soft.h and of lib_measure in measure.h. The similarity facts are C's
 *   table, added to the space as the (similar a b degree) atoms they are,
 *   and the model reads the same table. Each closeness, score, match and
 *   closest match is asked of the engine with the answer C's model computes,
 *   and a score whose pattern has a variable answers the binding C's walk
 *   made as well. The zoo is a space C opens and fills from its own table,
 *   so soft-match's answers are C's walk over that table, which feeds the
 *   measure algebra as the original's does. Declaring mean changes the
 *   space and C's model in the same step.
 * Guarantees: all twenty-five claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 * Build: cc 02-soft.c $(pkg-config --cflags --libs cmetta) -lm
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
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

static const similarity declared[] = { { "cat", "feline", 0.8 }, { "dog", "wolf", 0.7 } };
#define DECLARED (sizeof declared / sizeof *declared)

/* The engine's answer to CALL is C's score of P against A, or no answer
   where C's has none. TAKES all three. */
static void scores(metta *m, const soft *s, const char *claim, aggregation agg, mt_atom *call, mt_atom *p, mt_atom *a)
{
    soft_bindings bound = { NULL, 0 };
    double score;
    if (soft_score_by(s, agg, p, a, &bound, &score))
        assert(answers_are(mt_eval(m, call), E(score)) && claim);
    else
        assert(!mt_first(mt_eval(m, call)) && mt_ok() && claim);
    soft_bindings_free(&bound), mt_drop(p), mt_drop(a);
}
/* The same for soft-score under the space's own aggregation. TAKES both. */
static void score(metta *m, const soft *s, const char *claim, mt_atom *p, mt_atom *a)
{
    scores(m, s, claim, s->declared, E("soft-score", mt_keep(p), mt_keep(a)), p, a);
}

/* |x - target| < 1e-9 for C's score of P against A under AGG. TAKES both. */
static bool near(const soft *s, aggregation agg, mt_atom *p, mt_atom *a, double target)
{
    soft_bindings bound = { NULL, 0 };
    double score = 0;
    bool close = soft_score_by(s, agg, p, a, &bound, &score) && fabs(score - target) < 1.0e-9;
    soft_bindings_free(&bound), mt_drop(p), mt_drop(a);
    return close;
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("lib_measure", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_measure")))));
    require("lib_soft", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_soft")))));
    for (size_t i = 0; i < DECLARED; i++) require("a similarity", mt_add(m, E("similar", declared[i].a, declared[i].b, declared[i].degree)));
    soft model = { declared, DECLARED, SOFT_MIN };

    static const char *const pairs[][2] = { { "cat", "cat" }, { "cat", "feline" }, { "feline", "cat" }, { "cat", "dog" } };
    for (size_t i = 0; i < 4; i++) {
        mt_atom *a = S(pairs[i][0]), *b = S(pairs[i][1]);
        assert(answers_are(mt_eval(m, E("sym-sim", mt_keep(a), mt_keep(b))), E(sym_sim(&model, a, b))) && "sym-sim");
        mt_drop(a), mt_drop(b);
    }

    score(m, &model, "identical", E("likes", "cat", "fish"), E("likes", "cat", "fish"));
    score(m, &model, "one close symbol", E("likes", "feline", "fish"), E("likes", "cat", "fish"));
    score(m, &model, "the worst position", E("likes", "feline", "wolf"), E("likes", "cat", "dog"));
    score(m, &model, "lengths differ", E("likes", "cat"), E("likes", "cat", "fish"));
    score(m, &model, "heads differ", E("likes", "cat", "fish"), E("hates", "cat", "fish"));
    score(m, &model, "equal numbers", N(3), N(3));
    score(m, &model, "unequal numbers", N(3), N(4));
    score(m, &model, "an equation compared as written", E("=", E("likes", "cat", V("f")), V("body")),
          E("=", E("likes", "feline", "fish"), "tasty"));
    score(m, &model, "a runnable argument not run", E("likes", "cat", E("+", 1, 2)), E("likes", "feline", E("+", 1, 2)));
    score(m, &model, "a variable matches anything", V("x"), S("anything"));

    /* The binding is real: the walk binds $who, and the answer carries it. */
    mt_atom *pattern = E("likes", V("who"), "fish"), *candidate = E("likes", "cat", "fish");
    soft_bindings bound = { NULL, 0 };
    double degree = 0;
    require("C's walk scores it", soft_score(&model, pattern, candidate, &bound, &degree));
    mt_atom *who = V("who");
    assert(answers_are(mt_eval(m, E("let", V("probe"), E("soft-score", mt_keep(pattern), mt_keep(candidate)), E(V("probe"), V("who")))), E(E(degree, mt_keep(soft_bound(&bound, who)))))
           && "the score and the binding");
    soft_bindings_free(&bound), mt_drop(who), mt_drop(pattern), mt_drop(candidate);

    /* Soft matching over a space C fills, feeding the measure algebra. */
    mt_space *zoo_space = mt_space_open(m, "&zoo");
    require("&zoo", zoo_space != NULL);
    mt_atom *zoo[] = { E("likes", "cat", "fish"), E("likes", "dog", "bones"), E("likes", "bird", "seeds") };
    enum { ZOO = sizeof zoo / sizeof *zoo };
    for (size_t i = 0; i < ZOO; i++) require("a zoo atom", mt_add(zoo_space, mt_keep(zoo[i])));
    mt_atom *feline_fish = E("likes", "feline", "fish");
    ws matched = soft_match(&model, zoo, ZOO, feline_fish, 0.5);
    mt_atom **want = malloc((matched.n ? matched.n : 1) * sizeof *want);
    require("room", want != NULL);
    for (size_t i = 0; i < matched.n; i++) want[i] = pair_atom(&matched.pairs[i]);
    assert(answers_are(mt_eval(m, E("soft-match", S("&zoo"), mt_keep(feline_fish), 0.5)), mt_exprv(matched.n, want)) && "the matches above a half");
    free(want), ws_free(&matched);
    ws all = soft_match(&model, zoo, ZOO, feline_fish, 0.0);
    assert(answers_are(mt_eval(m, E("soft-best", S("&zoo"), mt_keep(feline_fish))), E(mt_keep(ws_best_pair(&all)->value))) && "the closest match");
    ws_free(&all), mt_drop(feline_fish);
    mt_atom *any = E("likes", V("x"), V("y"));
    ws every = soft_match(&model, zoo, ZOO, any, 0.0);
    assert(answers_are(mt_eval(m, E("let", V("answers"), E("collapse", E("soft-match", S("&zoo"), mt_keep(any), 0.0)), E("size-atom", V("answers")))), E((int64_t)every.n))
           && "every candidate is scored");
    ws_free(&every), mt_drop(any);
    mt_atom *feline_any = E("likes", "feline", V("f"));
    ws attention = soft_match(&model, zoo, ZOO, feline_any, 0.0), distribution = { NULL, 0 };
    require("a distribution", ws_softmax(&attention, 1.0, &distribution));
    assert(answers_are(mt_eval(m, E("<", E("abs-math", E("-", E("ws-total", E("ws-softmax", E("collapse", E("soft-match", S("&zoo"), mt_keep(feline_any), 0.0)), 1.0)), 1.0)), 1.0e-9)), E(B(fabs(ws_total(&distribution) - 1.0) < 1.0e-9)))
           && "attention over terms is a distribution");
    ws_free(&attention), ws_free(&distribution), mt_drop(feline_any);
    for (size_t i = 0; i < ZOO; i++) mt_drop(zoo[i]);
    mt_space_close(zoo_space);

    /* min is the worst position; mean keeps the two that agree. */
    score(m, &model, "under min one stranger is zero", E("likes", "cat", "fish"), E("likes", "dog", "fish"));
    assert(answers_are(mt_eval(m, E("<", E("abs-math", E("-", E("soft-score-by", "mean", E("likes", "cat", "fish"), E("likes", "dog", "fish")), 0.6666666666666666)), 1.0e-9)), E(B(near(&model, SOFT_MEAN, E("likes", "cat", "fish"), E("likes", "dog", "fish"), 0.6666666666666666))))
           && "under mean it is two thirds");
    scores(m, &model, "identical under mean", SOFT_MEAN, E("soft-score-by", "mean", E("likes", "cat", "fish"), E("likes", "cat", "fish")),
           E("likes", "cat", "fish"), E("likes", "cat", "fish"));

    /* The aggregation is the space's declaration, min where there is none. */
    assert(answers_are(mt_eval(m, E("soft-aggregation")), E(S(aggregation_names[model.declared]))) && "undeclared is min");
    require("declare mean", mt_add(m, E("soft-aggregate", aggregation_names[SOFT_MEAN])));
    model.declared = SOFT_MEAN;
    assert(answers_are(mt_eval(m, E("soft-aggregation")), E(S(aggregation_names[model.declared]))) && "declared is mean");
    assert(answers_are(mt_eval(m, E("<", E("abs-math", E("-", E("soft-score", E("likes", "cat", "fish"), E("likes", "dog", "fish")), 0.6666666666666666)), 1.0e-9)), E(B(near(&model, model.declared, E("likes", "cat", "fish"), E("likes", "dog", "fish"), 0.6666666666666666))))
           && "and every score reads it");
    mt_close(m);
    return 0;
}
