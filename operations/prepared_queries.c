/* Purpose: Retain a query pattern while facts and guards change.
 * Owns resources: local C handles are released before exit; a failed check
 *   terminates the example process.
 * Guarantees: the assertions below hold [tested: make check; commit=WORKTREE].
 * Open Obligations: None.
 */
#include "common.h"
int main(void)
{
    metta *m = open_engine();
    mt_atom *pattern = mt_expr("score", mt_var("who"), mt_var("n"));
    check("Ada score", mt_add(m, mt_expr("score", "Ada", 7)));
    check_answers("initial guarded result", mt_query(m, mt_keep(pattern), mt_expr(">", mt_var("n"), 5)), "(score Ada 7)");
    check("Bob score", mt_add(m, mt_expr("score", "Bob", 9)));
    check_answers("same pattern sees new facts", mt_query(m, mt_keep(pattern), mt_expr(">", mt_var("n"), 8)), "(score Bob 9)");
    mt_drop(pattern);
    return done(m, "prepared_queries");
}

