/* Purpose: the five operations the measure algebra is built from, each
 *   asked of lib_measure with the answer C's model of it in measure.h
 *   computes over C's tables: the ranking, the keep rule, the heavier of two
 *   pairs, the sampling walk with its budget given, and the merge of one
 *   pair into an accumulator. Where the original compares a surface
 *   operation with its composition, C compares the engine's two answers.
 * Guarantees: all eighteen claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
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

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("lib_measure", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_measure")))));

    ws graded = WS({ 0.2, "low" }, { 0.7, "high" }, { 0.1, "mid" }), none = { NULL, 0 };
    assert(answers_are(mt_eval(m, E("ws-ranked", ws_atom(&graded))), E(ws_atom_of(ws_ranked(&graded)))) && "ranked best-first");
    assert(answers_are(mt_eval(m, E("ws-ranked", mt_unit())), E(ws_atom_of(ws_ranked(&none)))) && "nothing ranks as nothing");

    ws ranked = ws_ranked(&graded), single = WS({ 0.7, "high" }), two = WS({ 0.7, "high" }, { 0.2, "low" });
    assert(answers_are(mt_eval(m, E("ws-take", ws_atom(&ranked), 2)), E(ws_atom_of(ws_take(&ranked, 2)))) && "the first two");
    assert(answers_are(mt_eval(m, E("ws-take", ws_atom(&single), 5)), E(ws_atom_of(ws_take(&single, 5)))) && "past the end is everything");
    assert(answers_are(mt_eval(m, E("ws-take", ws_atom(&two), 0)), E(ws_atom_of(ws_take(&two, 0)))) && "none is nothing");
    assert(answers_are(mt_eval(m, E("ws-take", mt_unit(), 2)), E(ws_atom_of(ws_take(&none, 2)))) && "from nothing, nothing");
    ws_free(&ranked), ws_free(&single), ws_free(&two);
    assert(atom_is(mt_one(mt_eval(m, E("ws-top", ws_atom(&graded), 2))), mt_one(mt_eval(m, E("ws-take", E("ws-ranked", ws_atom(&graded)), 2))))
           && "ws-top is ranking then taking");
    ws_free(&graded);

    /* The heavier of two, the first of two equal. */
    static const ws_row picks[][2] = { { { 0.2, "a" }, { 0.7, "b" } }, { { 0.7, "b" }, { 0.2, "a" } }, { { 0.5, "a" }, { 0.5, "b" } } };
    for (size_t i = 0; i < 3; i++) {
        ws pair = ws_of(picks[i], 2);
        assert(answers_are(mt_eval(m, E("ws-pickmax", pair_atom(&pair.pairs[0]), pair_atom(&pair.pairs[1]))), E(pair_atom(ws_pickmax(&pair.pairs[0], &pair.pairs[1]))))
               && "ws-pickmax");
        ws_free(&pair);
    }

    /* The walk under sampling, the budget given. */
    ws abc = WS({ 0.5, "a" }, { 0.3, "b" }, { 0.2, "c" });
    static const double budgets[] = { 0.1, 0.6, 0.9 };
    for (size_t i = 0; i < 3; i++)
        assert(answers_are(mt_eval(m, E("ws-sample-walk", ws_atom(&abc), budgets[i])), E(mt_keep(ws_sample_walk(&abc, budgets[i])))) && "ws-sample-walk");
    ws_free(&abc);
    ws halves = WS({ 0.5, "a" }, { 0.5, "b" });
    assert(answers_are(mt_eval(m, E("ws-sample-walk", ws_atom(&halves), 1.5)), E(mt_keep(ws_sample_walk(&halves, 1.5)))) && "the last pair takes the rest");
    ws_free(&halves);

    /* One pair merged into an accumulator. */
    static const struct {
        size_t n;
        ws_row acc[2], p;
    } merges[] = { { 0, { { 0 } }, { 0.3, "x" } }, { 2, { { 0.3, "x" }, { 0.4, "y" } }, { 0.2, "x" } }, { 1, { { 0.3, "x" } }, { 0.2, "z" } } };
    for (size_t i = 0; i < 3; i++) {
        ws acc = ws_of(merges[i].acc, merges[i].n), p = ws_of(&merges[i].p, 1);
        mt_atom *call = E("ws-merge-into", ws_atom(&acc), pair_atom(&p.pairs[0]));
        ws_merge_into(&acc, &p.pairs[0]);
        assert(answers_are(mt_eval(m, call), E(ws_atom_of(acc))) && "ws-merge-into");
        ws_free(&p);
    }
    ws xyx = WS({ 0.3, "x" }, { 0.4, "y" }, { 0.2, "x" });
    mt_atom *folded = mt_unit();
    for (size_t i = 0; i < xyx.n; i++) folded = E("ws-merge-into", folded, pair_atom(&xyx.pairs[i]));
    assert(atom_is(mt_one(mt_eval(m, E("ws-collapse", ws_atom(&xyx)))), mt_one(mt_eval(m, folded))) && "ws-collapse is the fold of the merge");
    ws_free(&xyx);
    mt_close(m);
    return 0;
}
