/* Purpose: solve, extend, solve again. reach is grounded one horizon at a
 *   time: each round asks whether d is reachable within t steps, and if not,
 *   adds the equation for t+1 inside a transaction, the way an incremental
 *   solver grounds one more part of a program between solves. An external
 *   fact is assigned and withdrawn between solves, and a grounding that fails
 *   rolls back whole.
 * Guarantees: d is first reached at horizon 3, and the failed grounding
 *   leaves nothing behind [tested 2026-09-27T00:35:58+10:00:
 *   make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>

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
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
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
    assert(horizon == 3 && "d is first reached at horizon 3");

    require("assign an external fact", mt_add(m, E("blocked", "c")));
    assert(answers_are(mt_match(m, E("blocked", V("x"))), E(E("blocked", "c"))) && "it is visible to the next solve");
    require("withdraw it", mt_del(m, E("blocked", "c")));
    assert(!mt_first(mt_match(m, E("blocked", V("x")))) && mt_ok() && "and gone after");

    assert(mt_transaction(m, abandoned_part, NULL) == MT_FAIL && "a failed part reports MT_FAIL");
    assert(!mt_first(mt_match(m, E("transient", V("x")))) && mt_ok() && "and leaves no fact");
    mt_close(m);
    return 0;
}
