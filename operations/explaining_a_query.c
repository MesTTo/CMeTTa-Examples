/* Purpose: ask how a query will run, then run it. explain answers the plan
 *   as atoms a program can read, here a generic trie join for a cyclic
 *   triangle, and the same query measured by the engine's counters answers
 *   the triangle's three rotations.
 * Guarantees: the plan names generic-join and the query answers three rows
 *   [tested: make check; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

/* (match &self (, (edge $x $y) (edge $y $z) (edge $z $x)) ($x $y $z)) */
static mt_atom *triangle(void)
{
    return E("match", "&self",
             E(",", E("edge", V("x"), V("y")), E("edge", V("y"), V("z")), E("edge", V("z"), V("x"))),
             E(V("x"), V("y"), V("z")));
}

int main(void)
{
    metta *m = open_engine();
    static const int64_t edges[][2] = { {1, 2}, {2, 3}, {3, 1} };
    for (size_t i = 0; i < 3; i++) require("store an edge", mt_add(m, E("edge", edges[i][0], edges[i][1])));
    mt_atom *set = mt_first(mt_eval(m, E("pragma!", "plan-cyclic-joins", B(true))));
    require("let the planner join cyclic patterns", mt_ok());
    mt_drop(set);

    mt_atom *plan = mt_one(mt_eval(m, E("explain", triangle())));
    require("explain the query", plan != NULL);
    bool generic_join = false;
    for (size_t i = 0; i < mt_len(plan); i++) {
        const mt_atom *item = mt_at(plan, i);
        const char *head = mt_name(mt_at(item, 0));
        if (head && strcmp(head, "plan") == 0)
            generic_join = mt_alpha_eq(mt_at(item, 1), S("generic-join"));
    }
    check("the plan is a generic trie join", generic_join);
    mt_drop(plan);

    mt_stats before = mt_stats_now(m);
    mt_list rows = mt_all(mt_eval(m, triangle()));
    mt_stats spent = mt_stats_since(before, mt_stats_now(m));
    check_int("the triangle's three rotations", (int64_t)rows.len, 3);
    check("priced in inferences", spent.inferences > 0);
    mt_list_free(rows);
    return done(m);
}
