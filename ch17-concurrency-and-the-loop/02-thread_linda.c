/* Purpose: Linda over a space. peek-atom is Linda's rd, waiting for a match
 *   and leaving it, and take-atom its in, removing exactly one; the
 *   non-blocking pair is C's own mt_match, Linda's rdp, and mt_del, its inp.
 *   A worker is a C loop taking one job per turn and summing what it took,
 *   checked against the jobs C wrote, and the rendezvous is a take that
 *   starts before the message exists while a C pthread, attached to the
 *   engine, writes it. inc, which the original defines, is a C function.
 * Guarantees: all ten claims of the original hold, with its three
 *   unasserted writes checked as well [tested 2026-09-27T00:35:58+10:00:
 *   make -C extensions/cmetta corpus-check].
 * Owns resources: one pthread, attached for its one write and joined.
 */
#define _POSIX_C_SOURCE 200809L
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include "_fixtures/thread_oracle.h"

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

static mt_atom *ref(mt_space *s) { return mt_spaceref(mt_space_name(s)); }

/* A job, (job n). */
static mt_atom *job(int64_t n) { return E("job", n); }

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    import_thread_lib(m);
    publish_unary(m, "inc", inc_op, NULL);
    mt_space *jobs = mt_space_open(m, "&jobs"), *work = mt_space_open(m, "&work"), *inbox = mt_space_open(m, "&inbox");
    require("open the three spaces", jobs && work && inbox);

    const int64_t posted = 7;
    assert(mt_add(jobs, job(posted)) && "a job is posted");
    for (int round = 0; round < 2; round++)
        assert(answers_are(mt_eval(m, E("peek-atom", ref(jobs), E("job", V("n")))), E(job(posted))) && "a peek leaves it");
    assert(answers_are(mt_eval(m, E("await-atom", ref(jobs), E("job", V("n")))), E(job(posted))) && "await-atom is the older name");
    assert(answers_are(mt_match(jobs, E("job", V("n"))), E(job(posted))) && "rdp is mt_match, which waits for nothing");
    assert(answers_are(mt_eval(m, E("take-atom", ref(jobs), E("job", V("n")))), E(job(posted))) && "a take removes the one it answers");
    assert(answers_are(mt_eval(m, E("collapse", E("take-atom", ref(jobs), E("job", V("n")), 0.05))), E(mt_unit()))
           && "so a second take gives up at its deadline");
    assert(!mt_first(mt_atoms(jobs)) && mt_ok() && "and the space is empty");

    const int64_t queued[] = { 1, 2 };
    const size_t n_queued = sizeof queued / sizeof *queued;
    int64_t expected = 0, done_work = 0;
    for (size_t i = 0; i < n_queued; i++) {
        assert(mt_add(work, job(queued[i])) && "a job is queued");
        expected += queued[i];
    }
    for (size_t i = 0; i < n_queued; i++) {
        mt_atom *taken = mt_first(mt_eval(m, E("take-atom", ref(work), E("job", V("a")), 1)));
        require("the worker takes a job", taken != NULL);
        done_work += mt_int(mt_at(taken, 1));
        mt_drop(taken);
    }
    assert(done_work == expected && "each take consumes its own job");
    assert(!mt_first(mt_atoms(work)) && mt_ok() && "so two takes drain two");
    mt_clear();
    assert(!mt_del(work, E("job", V("a"))) && mt_ok() && "and inp, mt_del, answers at once that nothing is left");

    writer w = { .space = inbox, .atom = E("msg", "hello") };
    start_writer(&w);
    assert(answers_are(mt_eval(m, E("take-atom", ref(inbox), E("msg", V("what")), 10)), E(mt_keep(w.atom)))
           && "a take blocks until another thread writes");
    join_writer(&w);
    assert(!mt_first(mt_atoms(inbox)) && mt_ok() && "and the message is taken");

    mt_space_close(jobs), mt_space_close(work), mt_space_close(inbox);
    mt_close(m);
    return 0;
}
