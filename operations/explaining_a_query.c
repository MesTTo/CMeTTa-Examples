/* Purpose: read a query plan as atoms and measure the query that it describes.
 * Owns resources: releases plan, answer collection and runtime.
 * Guarantees: explanations and measured answers are checked [tested: make check; commit=6022c3f48b6dc64752c6e49cfe9d985c7ac7a4e9].
 * Open Obligations: None.
 */
#include "common.h"
int main(void)
{
    metta *m = open_engine();
    check("triangle and planning policy", mt_do(m, "(edge 1 2) (edge 2 3) (edge 3 1) !(pragma! plan-cyclic-joins True)"));
    const char *goal = "(match &self (, (edge $x $y) (edge $y $z) (edge $z $x)) ($x $y $z))";
    mt_atom *plan = mt_one(mt_eval(m, mt_expr("explain", mt_parse(goal))));
    check("explanation is a collection", plan != NULL && mt_len(plan) > 0);
    bool saw_plan = false;
    for (size_t i = 0; i < mt_len(plan); ++i) {
        const mt_atom *item = mt_at(plan, i);
        const char *head = mt_name(mt_at(item, 0));
        if (head && strcmp(head, "plan") == 0) {
            check_atom("planner selected trie join", mt_at(item, 1), "generic-join"); saw_plan = true;
        }
    }
    check("plan was present", saw_plan); mt_drop(plan);
    mt_stats before = mt_stats_now(m);
    mt_list rows = mt_all(mt_eval(m, mt_parse(goal)));
    mt_stats work = mt_stats_since(before, mt_stats_now(m));
    check("triangle answers", mt_ok() && rows.len == 3); mt_list_free(rows);
    check("measured work", work.inferences > 0);
    return done(m, "explaining_a_query");
}
