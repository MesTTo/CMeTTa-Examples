/* Purpose: the five operations the measure algebra is built from, each
 *   asked of lib_measure with the answer C's model of it in measure.h
 *   computes over C's tables: the ranking, the keep rule, the heavier of two
 *   pairs, the sampling walk with its budget given, and the merge of one
 *   pair into an accumulator. Where the original compares a surface
 *   operation with its composition, C compares the engine's two answers.
 * Guarantees: all eighteen claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include "measure.h"

int main(void)
{
    metta *m = open_engine();
    require("lib_measure", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_measure")))));

    ws graded = WS({ 0.2, "low" }, { 0.7, "high" }, { 0.1, "mid" }), none = { NULL, 0 };
    check_answers("ranked best-first", mt_eval(m, E("ws-ranked", ws_atom(&graded))), ws_atom_of(ws_ranked(&graded)));
    check_answers("nothing ranks as nothing", mt_eval(m, E("ws-ranked", mt_unit())), ws_atom_of(ws_ranked(&none)));

    ws ranked = ws_ranked(&graded), single = WS({ 0.7, "high" }), two = WS({ 0.7, "high" }, { 0.2, "low" });
    check_answers("the first two", mt_eval(m, E("ws-take", ws_atom(&ranked), 2)), ws_atom_of(ws_take(&ranked, 2)));
    check_answers("past the end is everything", mt_eval(m, E("ws-take", ws_atom(&single), 5)), ws_atom_of(ws_take(&single, 5)));
    check_answers("none is nothing", mt_eval(m, E("ws-take", ws_atom(&two), 0)), ws_atom_of(ws_take(&two, 0)));
    check_answers("from nothing, nothing", mt_eval(m, E("ws-take", mt_unit(), 2)), ws_atom_of(ws_take(&none, 2)));
    ws_free(&ranked), ws_free(&single), ws_free(&two);
    check_atom("ws-top is ranking then taking", mt_one(mt_eval(m, E("ws-top", ws_atom(&graded), 2))),
               mt_one(mt_eval(m, E("ws-take", E("ws-ranked", ws_atom(&graded)), 2))));
    ws_free(&graded);

    /* The heavier of two, the first of two equal. */
    static const ws_row picks[][2] = { { { 0.2, "a" }, { 0.7, "b" } }, { { 0.7, "b" }, { 0.2, "a" } }, { { 0.5, "a" }, { 0.5, "b" } } };
    for (size_t i = 0; i < 3; i++) {
        ws pair = ws_of(picks[i], 2);
        check_answers("ws-pickmax", mt_eval(m, E("ws-pickmax", pair_atom(&pair.pairs[0]), pair_atom(&pair.pairs[1]))),
                      pair_atom(ws_pickmax(&pair.pairs[0], &pair.pairs[1])));
        ws_free(&pair);
    }

    /* The walk under sampling, the budget given. */
    ws abc = WS({ 0.5, "a" }, { 0.3, "b" }, { 0.2, "c" });
    static const double budgets[] = { 0.1, 0.6, 0.9 };
    for (size_t i = 0; i < 3; i++)
        check_answers("ws-sample-walk", mt_eval(m, E("ws-sample-walk", ws_atom(&abc), budgets[i])), mt_keep(ws_sample_walk(&abc, budgets[i])));
    ws_free(&abc);
    ws halves = WS({ 0.5, "a" }, { 0.5, "b" });
    check_answers("the last pair takes the rest", mt_eval(m, E("ws-sample-walk", ws_atom(&halves), 1.5)), mt_keep(ws_sample_walk(&halves, 1.5)));
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
        check_answers("ws-merge-into", mt_eval(m, call), ws_atom_of(acc));
        ws_free(&p);
    }
    ws xyx = WS({ 0.3, "x" }, { 0.4, "y" }, { 0.2, "x" });
    mt_atom *folded = mt_unit();
    for (size_t i = 0; i < xyx.n; i++) folded = E("ws-merge-into", folded, pair_atom(&xyx.pairs[i]));
    check_atom("ws-collapse is the fold of the merge", mt_one(mt_eval(m, E("ws-collapse", ws_atom(&xyx)))), mt_one(mt_eval(m, folded)));
    ws_free(&xyx);
    return done(m);
}
