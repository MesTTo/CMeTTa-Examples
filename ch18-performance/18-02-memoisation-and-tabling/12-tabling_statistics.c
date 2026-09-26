/* Purpose: what the incremental machinery did, counted. C keeps its own
 *   model of the statistics SWI keeps for the tabled reach, a table_stats
 *   from tabling.h, and moves it by the rule the original states: a write
 *   under a key the table read invalidates it, a write under any other key
 *   or head does not, and the next call re-evaluates an invalidated table
 *   and completes a second call. After each step the engine's table-stats
 *   must be the model's.
 * Guarantees: all seven claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include "_fixtures/tabling.h"

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

/* A write to &self, and the model's reading of it: whether reach's table
   read the key it lands under. */
static void write(metta *m, table_stats *model, mt_atom *atom, bool read_by_table)
{
    require("the write", mt_add(m, atom));
    if (read_by_table) model->invalidated++;
}

static void stats_are(metta *m, const char *claim, table_stats model)
{
    assert(answers_are(mt_eval(m, E("table-stats", E("reach", V("x"), V("y")))), E(table_stats_atom(model))) && claim);
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_tabling", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_tabling")))));
    require("(edge a b)", mt_add(m, E("edge", "a", "b")));
    require("reach", mt_add(m, E("=", E("reach", V("x"), V("y")), E("match", "&self", E("edge", V("x"), V("y")), V("y")))));
    require("table reach", mt_one_truth(mt_eval(m, E("tabled", E("reach", V("x"), V("y"))))));

    table_stats model = { .tables = 1, .answers = 1, .complete_call = 1, .declared = true };
    assert(answers_are(mt_eval(m, E("reach", "a", V("y"))), E(S("b"))) && "the first call answers from a fresh table");
    stats_are(m, "one call, one answer, nothing invalidated", model);
    write(m, &model, E("edge", "b", "d"), false);
    stats_are(m, "a key the call did not read leaves the table", model);
    write(m, &model, E("unrelated", "x", "y"), false);
    stats_are(m, "and so does another head", model);
    write(m, &model, E("edge", "a", "c"), true);
    stats_are(m, "a key the call read invalidates it", model);

    mt_list answers = mt_all(mt_eval(m, E("reach", "a", V("y"))));
    qsort(answers.items, answers.len, sizeof *answers.items, mt_order);
    assert(list_is(answers, E("b", "c")) && "and the next call answers afresh");
    model.answers = 2;
    model.complete_call++;
    model.reevaluated++;
    stats_are(m, "re-evaluating it once", model);
    mt_close(m);
    return 0;
}
