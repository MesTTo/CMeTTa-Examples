/* Purpose: what the incremental machinery did, counted. C keeps its own
 *   model of the statistics SWI keeps for the tabled reach, a table_stats
 *   from tabling.h, and moves it by the rule the original states: a write
 *   under a key the table read invalidates it, a write under any other key
 *   or head does not, and the next call re-evaluates an invalidated table
 *   and completes a second call. After each step the engine's table-stats
 *   must be the model's.
 * Guarantees: all seven claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include "tabling.h"

/* A write to &self, and the model's reading of it: whether reach's table
   read the key it lands under. */
static void write(metta *m, table_stats *model, mt_atom *atom, bool read_by_table)
{
    require("the write", mt_add(m, atom));
    if (read_by_table) model->invalidated++;
}

static void stats_are(metta *m, const char *claim, table_stats model)
{
    check_answers(claim, mt_eval(m, E("table-stats", E("reach", V("x"), V("y")))), table_stats_atom(model));
}

int main(void)
{
    metta *m = open_engine();
    require("import lib_tabling", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_tabling")))));
    require("(edge a b)", mt_add(m, E("edge", "a", "b")));
    require("reach", mt_add(m, E("=", E("reach", V("x"), V("y")), E("match", "&self", E("edge", V("x"), V("y")), V("y")))));
    require("table reach", mt_one_truth(mt_eval(m, E("tabled", E("reach", V("x"), V("y"))))));

    table_stats model = { .tables = 1, .answers = 1, .complete_call = 1, .declared = true };
    check_answers("the first call answers from a fresh table", mt_eval(m, E("reach", "a", V("y"))), S("b"));
    stats_are(m, "one call, one answer, nothing invalidated", model);
    write(m, &model, E("edge", "b", "d"), false);
    stats_are(m, "a key the call did not read leaves the table", model);
    write(m, &model, E("unrelated", "x", "y"), false);
    stats_are(m, "and so does another head", model);
    write(m, &model, E("edge", "a", "c"), true);
    stats_are(m, "a key the call read invalidates it", model);

    mt_list answers = mt_all(mt_eval(m, E("reach", "a", V("y"))));
    qsort(answers.items, answers.len, sizeof *answers.items, mt_order);
    check_list("and the next call answers afresh", answers, "b", "c");
    model.answers = 2;
    model.complete_call++;
    model.reevaluated++;
    stats_are(m, "re-evaluating it once", model);
    return done(m);
}
