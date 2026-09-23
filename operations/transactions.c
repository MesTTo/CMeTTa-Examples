/* Purpose: Commit, roll back and speculate through one closed scope.
 * Owns resources: local C handles are released before exit; a failed check
 *   terminates the example process.
 * Guarantees: the assertions below hold [tested: make check; commit=WORKTREE].
 * Open Obligations: None.
 */
#include "common.h"
typedef struct { mt_space *space; mt_status verdict; } work;
static mt_status write_pair(metta *m, void *user)
{
    work *w = user; (void)m;
    if (!mt_add(w->space, mt_expr("edge", 1, 2)) ||
        !mt_add(w->space, mt_expr("edge", 2, 3))) return mt_error();
    return w->verdict;
}
int main(void)
{
    metta *m = open_engine();
    work w = {mt_self(m), MT_FAIL};
    check("rollback verdict", mt_transaction(m, write_pair, &w) == MT_FAIL);
    check("rollback has no rows", mt_count(m) == 0);
    w.verdict = MT_OK;
    check("speculation succeeds", mt_speculate(m, write_pair, &w) == MT_OK);
    check("speculation discards rows", mt_count(m) == 0);
    check("commit succeeds", mt_transaction(m, write_pair, &w) == MT_OK);
    check_answers("commit publishes both rows", mt_atoms(m), "(edge 1 2) (edge 2 3)");
    return done(m, "transactions");
}

