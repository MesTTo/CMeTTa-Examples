/* Purpose: one callback, three verdicts. The same C function writes two
 *   edges; run under mt_transaction() it commits when it answers MT_OK and
 *   rolls back when it answers MT_FAIL, and under mt_speculate() it always
 *   rolls back.
 * Guarantees: rollback and speculation leave nothing, commit publishes both
 *   edges [tested: make check; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

typedef struct work { mt_space *space; mt_status verdict; } work;

static mt_status write_pair(metta *m, void *user)
{
    (void)m;
    work *w = user;
    if (!mt_add(w->space, E("edge", 1, 2)) || !mt_add(w->space, E("edge", 2, 3)))
        return mt_error();
    return w->verdict;
}

int main(void)
{
    metta *m = open_engine();
    work w = { mt_self(m), MT_FAIL };
    check("MT_FAIL rolls back", mt_transaction(m, write_pair, &w) == MT_FAIL);
    check_int("and nothing was written", (int64_t)mt_count(m), 0);
    w.verdict = MT_OK;
    check("speculation succeeds", mt_speculate(m, write_pair, &w) == MT_OK);
    check_int("and discards its writes anyway", (int64_t)mt_count(m), 0);
    check("MT_OK commits", mt_transaction(m, write_pair, &w) == MT_OK);
    check_answers("both edges are published together", mt_atoms(m), E("edge", 1, 2), E("edge", 2, 3));
    return done(m);
}
