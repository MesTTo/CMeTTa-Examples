/* Purpose: Run source, construct terms and query joined facts.
 * Owns resources: local C handles are released before exit; a failed check
 *   terminates the example process.
 * Guarantees: the assertions below hold [tested: make check; commit=WORKTREE].
 * Open Obligations: None.
 */
#include "common.h"
int main(void)
{
    metta *m = open_engine();
    check_answers("double 21", mt_run(m, "(= (double $x) (* $x 2)) !(double 21)"), "42");
    check("Tom to Bob", mt_add(m, mt_expr("Parent", "Tom", "Bob")));
    check("Bob to Ann", mt_add(m, mt_expr("Parent", "Bob", "Ann")));
    check("Ann to Zoe", mt_add(m, mt_expr("Parent", "Ann", "Zoe")));
    mt_atom *join = mt_expr(",", mt_expr("Parent", mt_var("gp"), mt_var("p")),
                                   mt_expr("Parent", mt_var("p"), mt_var("gc")));
    size_t rows = 0;
    mt_rows(row, mt_query(m, join, NULL)) {
        if (rows == 0) {
            check_atom("first grandparent", mt_bound(row, "gp"), "Tom");
            check_atom("first grandchild", mt_bound(row, "gc"), "Ann");
        }
        ++rows;
    }
    check("two joined rows", rows == 2);
    check_answers("nondeterministic values", mt_eval(m, mt_expr("superpose", mt_expr(1, 2, 3))), "1 2 3");
    return done(m, "first_steps");
}

