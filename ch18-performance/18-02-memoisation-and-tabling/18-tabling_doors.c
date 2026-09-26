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
 * Guarantees: all twenty-three claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

static metta *engine;

static mt_atom *reach(void) { return E("reach", V("x"), V("y")); }
static mt_atom *quoted_reach(void) { return E("quote", reach()); }

static void answers_b(const char *claim)
{
    assert(answers_are(mt_eval(engine, E("reach", "a", V("y"))), E(S("b"))) && claim);
}

static void truly(const char *claim, mt_atom *goal)
{
    assert(answers_are(mt_eval(engine, goal), E(B(true))) && claim);
}

/* What the injected clauses hold, decided in C: a greeting for world only,
   and metta_doors_add(X, Y) exactly when Y is X + 1. */
static bool greets(const char *who) { return strcmp(who, "world") == 0; }
static bool adds(int64_t x, int64_t y) { return y == x + 1; }

int main(void)
{
    metta *m = engine = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_tabling", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_tabling")))));
    require("import lib_spaces", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_spaces")))));
    require("(edge a b)", mt_add(m, E("edge", "a", "b")));
    require("reach", mt_add(m, E("=", E("reach", V("x"), V("y")), E("match", "&self", E("edge", V("x"), V("y")), V("y")))));
    require("table reach", mt_one_truth(mt_eval(m, E("tabled", reach()))));

    answers_b("the table answers b");
    table_stats stats = { .tables = 1, .answers = 1, .complete_call = 1, .declared = true };
    assert(answers_are(mt_eval(m, E("table-stats", reach())), E(table_stats_atom(stats)))
           && "a standing declaration carries its policy");
    truly("untabled removes the instrumentation", E("untabled", reach()));
    stats = (table_stats){ .tables = 1, .complete_call = 1 };
    assert(answers_are(mt_eval(m, E("table-stats", reach())), E(table_stats_atom(stats)))
           && "SWI keeps the counters and the policy goes");
    truly("declaring again", E("tabled", reach()));
    truly("is idempotent", E("tabled", reach()));
    stats.declared = true;
    mt_atom *report = table_stats_atom(stats);
    assert(answers_are(mt_eval(m, E("index-atom", E("table-stats", reach()), 5)), E(mt_keep(mt_at(report, 5)))) && "the policy is back");
    mt_drop(report);

    answers_b("the table answers again");
    truly("table-clear-all abolishes every table", E("table-clear-all"));
    assert(answers_are(mt_eval(m, E("index-atom", E("table-stats", reach()), 1)), E(E("answers", 0)))
           && "so reach's holds no answers");

    truly("the Prolog predicate untabling a quoted call", E("metta_untabled_decl", quoted_reach()));
    truly("and the one tabling it", E("metta_tabled_decl", quoted_reach()));
    answers_b("the table answers once more");
    truly("the MeTTa statistics are the Prolog predicate's",
          E("==", E("metta_table_statistics", quoted_reach()), E("table-stats", reach())));
    truly("clearing one call's table", E("metta_table_clear", quoted_reach()));
    assert(answers_are(mt_eval(m, E("index-atom", E("metta_table_statistics", quoted_reach()), 1)), E(E("answers", 0)))
           && "empties it");
    truly("and clearing them all", E("metta_table_clear_all"));

    truly("Prolog source loads", E("injectPrologCode", mt_text("metta_doors_greeting(world).")));
    static const char *const greeted[] = { "world", "mars" };
    for (size_t i = 0; i < 2; i++)
        assert(answers_are(mt_eval(m, E("succeedsPredicate", E("metta_doors_greeting", greeted[i]))), E(B(greets(greeted[i]))))
               && "the fact holds for world alone");
    truly("a clause loads too", E("injectPrologCode", mt_text("metta_doors_add(X, Y) :- Y is X + 1.")));
    static const int64_t sums[] = { 42, 43 };
    for (size_t i = 0; i < 2; i++)
        assert(answers_are(mt_eval(m, E("succeedsPredicate", E("metta_doors_add", 41, sums[i]))), E(B(adds(41, sums[i]))))
               && "the clause holds for x + 1 alone");
    mt_close(m);
    return 0;
}
