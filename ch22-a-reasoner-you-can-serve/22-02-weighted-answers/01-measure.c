/* Purpose: the measure algebra over weighted alternatives, each operation
 *   asked of lib_measure with the answer C's model of it in measure.h
 *   computes: the total, the distribution, the best value, the top two, the
 *   collapse, the expectation, the filter and the flip; softmax at a cold, a
 *   hot and a middling temperature; the peak; and the nondeterministic
 *   reading, whose answers are the pairs themselves. The superpositions are
 *   C's tables. The program's two helpers, the first weight and both
 *   weights, are the equations they are, and C reads the same weights off
 *   its own softmax, which is how it knows the one-unit gap answers the same
 *   distribution at 1.0, at 1000.0 and at -1000.0. A sample is random, so C
 *   holds it to be a value its table carries, and a one-pair walk answers
 *   its value for any budget.
 * Guarantees: all nineteen claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 * Build: cc 01-measure.c $(pkg-config --cflags --libs cmetta) -lm
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include "_fixtures/measure.h"

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

/* A softmax C computes, which must exist. */
static ws softmax(ws s, double temperature)
{
    ws out = { NULL, 0 };
    require("a distribution", ws_softmax(&s, temperature, &out));
    ws_free(&s);
    return out;
}

static bool carries(const ws *s, const mt_atom *value)
{
    for (size_t i = 0; i < s->n; i++)
        if (mt_eq(s->pairs[i].value, value)) return true;
    return false;
}

/* A drawn value, which must be one the table carries. CONSUMES the table. */
static void draws_from(metta *m, const char *claim, ws s)
{
    mt_atom *drawn = mt_one(mt_eval(m, E("ws-sample!", ws_atom(&s))));
    assert(drawn && carries(&s, drawn) && claim);
    mt_drop(drawn), ws_free(&s);
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("lib_measure", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_measure")))));

    ws s = WS({ 0.5, "a" }, { 0.25, "b" }, { 0.25, "c" });
    assert(answers_are(mt_eval(m, E("ws-total", ws_atom(&s))), E(ws_total(&s))) && "the total");
    ws_free(&s);
    s = WS({ 2.0, "a" }, { 2.0, "b" });
    ws distribution = { NULL, 0 };
    require("a distribution", ws_normalize(&s, &distribution));
    assert(answers_are(mt_eval(m, E("ws-normalize", ws_atom(&s))), E(ws_atom_of(distribution))) && "normalized");
    ws_free(&s);
    ws graded = WS({ 0.2, "low" }, { 0.7, "high" }, { 0.1, "mid" });
    assert(answers_are(mt_eval(m, E("ws-best", ws_atom(&graded))), E(mt_keep(ws_best_pair(&graded)->value))) && "the best");
    assert(answers_are(mt_eval(m, E("ws-top", ws_atom(&graded), 2)), E(ws_atom_of(ws_top(&graded, 2)))) && "the top two");
    ws_free(&graded);
    s = WS({ 0.3, "x" }, { 0.4, "y" }, { 0.2, "x" });
    assert(answers_are(mt_eval(m, E("ws-collapse", ws_atom(&s))), E(ws_atom_of(ws_collapse(&s)))) && "collapsed");
    ws_free(&s);
    ws numeric = { NULL, 0 };
    ws_push(&numeric, 0.5, N(10)), ws_push(&numeric, 0.5, N(20));
    assert(answers_are(mt_eval(m, E("ws-expect", ws_atom(&numeric))), E(ws_expect(&numeric))) && "the expectation");
    ws_free(&numeric);
    s = WS({ 0.9, "keep" }, { 0.05, "drop" });
    assert(answers_are(mt_eval(m, E("ws-filter", ws_atom(&s), 0.1)), E(ws_atom_of(ws_filter(&s, 0.1)))) && "filtered");
    ws_free(&s);
    static const ws_row scored[] = { { 0.9, "cat" }, { 0.4, "dog" } };
    assert(answers_are(mt_eval(m, E("ws-flip", E(E(scored[0].value, scored[0].weight), E(scored[1].value, scored[1].weight)))), E(ws_atom_of(ws_of(scored, 2))))
           && "flipped");

    /* The program's helpers, and C's reading of the same weights. */
    require("first-weight", mt_add(m, E("=", E("first-weight", V("ps")),
                                       E("let", V("head"), E("car-atom", V("ps")), E("index-atom", V("head"), 0)))));
    require("both-weights", mt_add(m, E("=", E("both-weights", V("ps")),
                                       E(E("first-weight", V("ps")), E("first-weight", E("cdr-atom", V("ps")))))));
    ws cold = softmax(WS({ 1.0, "low" }, { 3.0, "high" }), 0.1);
    assert(answers_are(mt_eval(m, E("ws-best", E("ws-softmax", E(E(1.0, "low"), E(3.0, "high")), 0.1))), E(mt_keep(ws_best_pair(&cold)->value)))
           && "cold picks the argmax");
    ws_free(&cold);
    ws sharp = softmax(WS({ 1.0, "a" }, { 3.0, "b" }), 0.1), flat = softmax(WS({ 1.0, "a" }, { 3.0, "b" }), 1000.0);
    assert(answers_are(mt_eval(m, E(">", E("first-weight", E("ws-softmax", E(E(1.0, "a"), E(3.0, "b")), 0.1)), 0.0)), E(B(sharp.pairs[0].weight > 0.0)))
           && "cold keeps some weight");
    assert(answers_are(mt_eval(m, E("<", E("abs-math", E("-", E("first-weight", E("ws-softmax", E(E(1.0, "a"), E(3.0, "b")), 1000.0)), 0.5)), 0.01)), E(B(fabs(flat.pairs[0].weight - 0.5) < 0.01)))
           && "hot flattens");
    ws_free(&sharp), ws_free(&flat);
    ws three = softmax(WS({ 2.0, "a" }, { 5.0, "b" }, { 1.0, "c" }), 0.7);
    assert(answers_are(mt_eval(m, E("<", E("abs-math", E("-", E("ws-total", E("ws-softmax", E(E(2.0, "a"), E(5.0, "b"), E(1.0, "c")), 0.7)), 1.0)), 1.0e-9)), E(B(fabs(ws_total(&three) - 1.0) < 1.0e-9)))
           && "a softmax is a distribution");
    ws_free(&three);
    static const double shifts[][2] = { { 1000.0, 1001.0 }, { -1000.0, -999.0 } };
    for (size_t i = 0; i < 2; i++) {
        ws at_one = softmax(WS({ 1.0, "a" }, { 2.0, "b" }), 1.0), shifted = softmax(WS({ shifts[i][0], "a" }, { shifts[i][1], "b" }), 1.0);
        require("C's weights agree at every size",
                at_one.pairs[0].weight == shifted.pairs[0].weight && at_one.pairs[1].weight == shifted.pairs[1].weight);
        assert(atom_is(mt_one(mt_eval(m, E("both-weights", E("ws-softmax", E(E(1.0, "a"), E(2.0, "b")), 1.0)))), mt_one(mt_eval(m, E("both-weights", E("ws-softmax", E(E(shifts[i][0], "a"), E(shifts[i][1], "b")), 1.0)))))
               && "the same gap, the same distribution");
        ws_free(&at_one), ws_free(&shifted);
    }
    s = WS({ 1.0, "a" }, { 7.0, "b" }, { 3.0, "c" });
    assert(answers_are(mt_eval(m, E("ws-peak", ws_atom(&s))), E(ws_best_pair(&s)->weight)) && "the peak");
    ws_free(&s);

    /* Sampling draws only values the superposition carries. */
    draws_from(m, "a coin lands on a face", WS({ 0.5, "heads" }, { 0.5, "tails" }));
    s = WS({ 1.0, "sure" });
    assert(answers_are(mt_eval(m, E("ws-sample!", ws_atom(&s))), E(mt_keep(ws_sample_walk(&s, 0.0)))) && "one pair is sure");
    ws_free(&s);
    draws_from(m, "a skewed draw lands on a value", WS({ 0.1, "a" }, { 0.2, "b" }, { 0.7, "c" }));

    s = WS({ 0.6, "yes" }, { 0.4, "no" });
    assert(answers_are(mt_eval(m, E("ws-choose", ws_atom(&s))), E(pair_atom(&s.pairs[0]), pair_atom(&s.pairs[1]))) && "each alternative is an answer");
    ws_free(&s);
    mt_close(m);
    return 0;
}
