/* Purpose: the rest of lib_tabling, and the Prolog predicates under it.
 *   A declaration installs a table whose statistics carry the policy while
 *   it stands; untabled removes the instrumentation and the policy pair with
 *   it, while SWI keeps the table's own counters; declaring is idempotent;
 *   table-clear-all abolishes every table. Each report is built by tabling.h
 *   from the counts C expects at that step. The MeTTa names are one equation
 *   each over five Prolog predicates taking the call quoted, reached the same
 *   way. injectPrologCode loads Prolog SOURCE, which is text, so C passes it
 *   as text, and C decides what the loaded predicates must answer: the one
 *   greeting fact, and y = x + 1 for the add clause.
 * Guarantees: all twenty-three claims of the original hold [tested: make
 *   twins; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include "tabling.h"

static metta *engine;

static mt_atom *reach(void) { return E("reach", V("x"), V("y")); }
static mt_atom *quoted_reach(void) { return E("quote", reach()); }

static void answers_b(const char *claim)
{
    check_answers(claim, mt_eval(engine, E("reach", "a", V("y"))), S("b"));
}

static void truly(const char *claim, mt_atom *goal)
{
    check_answers(claim, mt_eval(engine, goal), B(true));
}

/* What the injected clauses hold, decided in C: a greeting for world only,
   and metta_doors_add(X, Y) exactly when Y is X + 1. */
static bool greets(const char *who) { return strcmp(who, "world") == 0; }
static bool adds(int64_t x, int64_t y) { return y == x + 1; }

int main(void)
{
    metta *m = engine = open_engine();
    require("import lib_tabling", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_tabling")))));
    require("import lib_spaces", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_spaces")))));
    require("(edge a b)", mt_add(m, E("edge", "a", "b")));
    require("reach", mt_add(m, E("=", E("reach", V("x"), V("y")), E("match", "&self", E("edge", V("x"), V("y")), V("y")))));
    require("table reach", mt_one_truth(mt_eval(m, E("tabled", reach()))));

    answers_b("the table answers b");
    table_stats stats = { .tables = 1, .answers = 1, .complete_call = 1, .declared = true };
    check_answers("a standing declaration carries its policy", mt_eval(m, E("table-stats", reach())),
                  table_stats_atom(stats));
    truly("untabled removes the instrumentation", E("untabled", reach()));
    stats = (table_stats){ .tables = 1, .complete_call = 1 };
    check_answers("SWI keeps the counters and the policy goes", mt_eval(m, E("table-stats", reach())),
                  table_stats_atom(stats));
    truly("declaring again", E("tabled", reach()));
    truly("is idempotent", E("tabled", reach()));
    stats.declared = true;
    mt_atom *report = table_stats_atom(stats);
    check_answers("the policy is back", mt_eval(m, E("index-atom", E("table-stats", reach()), 5)), mt_keep(mt_at(report, 5)));
    mt_drop(report);

    answers_b("the table answers again");
    truly("table-clear-all abolishes every table", E("table-clear-all"));
    check_answers("so reach's holds no answers", mt_eval(m, E("index-atom", E("table-stats", reach()), 1)),
                  E("answers", 0));

    truly("the Prolog predicate untabling a quoted call", E("metta_untabled_decl", quoted_reach()));
    truly("and the one tabling it", E("metta_tabled_decl", quoted_reach()));
    answers_b("the table answers once more");
    truly("the MeTTa statistics are the Prolog predicate's",
          E("==", E("metta_table_statistics", quoted_reach()), E("table-stats", reach())));
    truly("clearing one call's table", E("metta_table_clear", quoted_reach()));
    check_answers("empties it", mt_eval(m, E("index-atom", E("metta_table_statistics", quoted_reach()), 1)),
                  E("answers", 0));
    truly("and clearing them all", E("metta_table_clear_all"));

    truly("Prolog source loads", E("injectPrologCode", mt_text("metta_doors_greeting(world).")));
    static const char *const greeted[] = { "world", "mars" };
    for (size_t i = 0; i < 2; i++)
        check_answers("the fact holds for world alone", mt_eval(m, E("succeedsPredicate", E("metta_doors_greeting", greeted[i]))),
                      B(greets(greeted[i])));
    truly("a clause loads too", E("injectPrologCode", mt_text("metta_doors_add(X, Y) :- Y is X + 1.")));
    static const int64_t sums[] = { 42, 43 };
    for (size_t i = 0; i < 2; i++)
        check_answers("the clause holds for x + 1 alone", mt_eval(m, E("succeedsPredicate", E("metta_doors_add", 41, sums[i]))),
                      B(adds(41, sums[i])));
    return done(m);
}
