/* Purpose: Share a mutable engine cell through retained C atoms.
 * Owns resources: local C handles are released before exit; a failed check
 *   terminates the example process.
 * Guarantees: the assertions below hold [tested: make check; commit=WORKTREE].
 * Open Obligations: None.
 */
#include "common.h"
int main(void)
{
    metta *m = open_engine();
    mt_atom *cell = mt_one(mt_eval(m, mt_expr("new-state", 0)));
    check("cell created", cell != NULL);
    check_answers("initial state", mt_eval(m, mt_expr("get-state", mt_keep(cell))), "0");
    check_answers("change state", mt_eval(m, mt_expr("change-state!", mt_keep(cell), 7)), "True");
    check_answers("updated state", mt_eval(m, mt_expr("get-state", mt_keep(cell))), "7");
    mt_drop(cell);
    return done(m, "cells");
}

