/* Purpose: Linda over a space. peek-atom is Linda's rd, waiting for a match
 *   and leaving it, and take-atom its in, removing exactly one; the
 *   non-blocking pair is C's own mt_match, Linda's rdp, and mt_del, its inp.
 *   A worker is a C loop taking one job per turn and summing what it took,
 *   checked against the jobs C wrote, and the rendezvous is a take that
 *   starts before the message exists while a C pthread, attached to the
 *   engine, writes it. inc, which the original defines, is a C function.
 * Guarantees: all ten claims of the original hold, with its three
 *   unasserted writes checked as well [tested: make twins; commit=WORKTREE].
 * Owns resources: one pthread, attached for its one write and joined.
 */
#define MT_SHORTHAND
#include "common.h"
#include "thread_oracle.h"

static mt_atom *ref(mt_space *s) { return mt_spaceref(mt_space_name(s)); }

/* A job, (job n). */
static mt_atom *job(int64_t n) { return E("job", n); }

int main(void)
{
    metta *m = open_engine();
    import_thread_lib(m);
    publish_unary(m, "inc", inc_op, NULL);
    mt_space *jobs = mt_space_open(m, "&jobs"), *work = mt_space_open(m, "&work"), *inbox = mt_space_open(m, "&inbox");
    require("open the three spaces", jobs && work && inbox);

    const int64_t posted = 7;
    check("a job is posted", mt_add(jobs, job(posted)));
    for (int round = 0; round < 2; round++)
        check_answers("a peek leaves it", mt_eval(m, E("peek-atom", ref(jobs), E("job", V("n")))), job(posted));
    check_answers("await-atom is the older name", mt_eval(m, E("await-atom", ref(jobs), E("job", V("n")))), job(posted));
    check_answers("rdp is mt_match, which waits for nothing", mt_match(jobs, E("job", V("n"))), job(posted));
    check_answers("a take removes the one it answers", mt_eval(m, E("take-atom", ref(jobs), E("job", V("n")))), job(posted));
    check_answers("so a second take gives up at its deadline", mt_eval(m, E("collapse", E("take-atom", ref(jobs), E("job", V("n")), 0.05))),
                  mt_unit());
    check_none("and the space is empty", mt_atoms(jobs));

    const int64_t queued[] = { 1, 2 };
    const size_t n_queued = sizeof queued / sizeof *queued;
    int64_t expected = 0, done_work = 0;
    for (size_t i = 0; i < n_queued; i++) {
        check("a job is queued", mt_add(work, job(queued[i])));
        expected += queued[i];
    }
    for (size_t i = 0; i < n_queued; i++) {
        mt_atom *taken = mt_first(mt_eval(m, E("take-atom", ref(work), E("job", V("a")), 1)));
        require("the worker takes a job", taken != NULL);
        done_work += mt_int(mt_at(taken, 1));
        mt_drop(taken);
    }
    check_int("each take consumes its own job", done_work, expected);
    check_none("so two takes drain two", mt_atoms(work));
    mt_clear();
    check("and inp, mt_del, answers at once that nothing is left", !mt_del(work, E("job", V("a"))) && mt_ok());

    writer w = { .space = inbox, .atom = E("msg", "hello") };
    start_writer(&w);
    check_answers("a take blocks until another thread writes", mt_eval(m, E("take-atom", ref(inbox), E("msg", V("what")), 10)),
                  mt_keep(w.atom));
    join_writer(&w);
    check_none("and the message is taken", mt_atoms(inbox));

    mt_space_close(jobs), mt_space_close(work), mt_space_close(inbox);
    return done(m);
}
