/* Purpose: one callback, three verdicts. The same C function writes two
 *   edges; run under mt_transaction() it commits when it answers MT_OK and
 *   rolls back when it answers MT_FAIL, and under mt_speculate() it always
 *   rolls back.
 * Guarantees: rollback and speculation leave nothing, commit publishes both
 *   edges [tested 2026-09-27T00:35:58+10:00:
 *   make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>

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
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    work w = { mt_self(m), MT_FAIL };
    assert(mt_transaction(m, write_pair, &w) == MT_FAIL && "MT_FAIL rolls back");
    assert((int64_t)mt_count(m) == 0 && "and nothing was written");
    w.verdict = MT_OK;
    assert(mt_speculate(m, write_pair, &w) == MT_OK && "speculation succeeds");
    assert((int64_t)mt_count(m) == 0 && "and discards its writes anyway");
    assert(mt_transaction(m, write_pair, &w) == MT_OK && "MT_OK commits");
    assert(answers_are(mt_atoms(m), E(E("edge", 1, 2), E("edge", 2, 3))) && "both edges are published together");
    mt_close(m);
    return 0;
}
