/* Purpose: solve, extend, solve again. reach is grounded one horizon at a
 *   time: each round asks whether d is reachable within t steps, and if not,
 *   adds the equation for t+1 inside a transaction, the way an incremental
 *   solver grounds one more part of a program between solves. An external
 *   fact is assigned and withdrawn between solves, and a grounding that fails
 *   rolls back whole.
 * Guarantees: d is first reached at horizon 3, and the failed grounding
 *   leaves nothing behind [tested: make check; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

/* (= (reach $x t) (match &self (edge $y $x) (once (reach $y t-1)))) */
static mt_status ground_horizon(metta *m, void *user)
{
    int64_t t = *(const int64_t *)user;
    mt_atom *rule = E("=", E("reach", V("x"), t),
                      E("match", "&self", E("edge", V("y"), V("x")),
                        E("once", E("reach", V("y"), t - 1))));
    return mt_add(m, rule) ? MT_OK : mt_error();
}

static mt_status abandoned_part(metta *m, void *user)
{
    (void)user;
    if (!mt_add(m, E("transient", 1))) return mt_error();
    return MT_FAIL;                      /* roll the part back */
}

int main(void)
{
    metta *m = open_engine();
    require("reach answers nothing where no equation matches",
            mt_add(mt_catalog(m), E("dispatch-policy", "reach", "NoMatchEnum", "NoMatchFail")));
    static const char *const edges[][2] = { {"a", "b"}, {"b", "c"}, {"c", "d"} };
    for (size_t i = 0; i < 3; i++) require("store an edge", mt_add(m, E("edge", edges[i][0], edges[i][1])));
    require("(= (reach a 0) True)", mt_add(m, E("=", E("reach", "a", 0), B(true))));

    int64_t horizon = 0;
    for (;;) {
        mt_list reached = mt_all(mt_eval(m, E("reach", "d", horizon)));
        require("solve", mt_ok());
        bool done_here = reached.len != 0;
        mt_list_free(reached);
        if (done_here || horizon == 3) break;
        horizon++;
        require("ground the next horizon", mt_transaction(m, ground_horizon, &horizon) == MT_OK);
    }
    check_int("d is first reached at horizon 3", horizon, 3);

    require("assign an external fact", mt_add(m, E("blocked", "c")));
    check_answers("it is visible to the next solve", mt_match(m, E("blocked", V("x"))), E("blocked", "c"));
    require("withdraw it", mt_del(m, E("blocked", "c")));
    check_none("and gone after", mt_match(m, E("blocked", V("x"))));

    check("a failed part reports MT_FAIL", mt_transaction(m, abandoned_part, NULL) == MT_FAIL);
    check_none("and leaves no fact", mt_match(m, E("transient", V("x"))));
    return done(m);
}
