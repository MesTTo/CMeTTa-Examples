/* Purpose: lib_thread's lower rung, the underscore spellings of the Prolog
 *   predicates its MeTTa names are equations over, each held against the
 *   same C oracle as its hyphenated twin: C's map, filter, all and any over
 *   the C array the call was given, inc's value for what a future computes,
 *   the words C sent through a channel, C's record of an idle pool. Where
 *   the original asks whether an underscore spelling agrees with its MeTTa
 *   name, C compares the two answers the engine gives it, and cpu_count is
 *   sysconf(_SC_NPROCESSORS_ONLN) as cpu-count is. The Linda pair with a
 *   where-guard is held against C's own model of the jobs it wrote: the first
 *   job the guard's threshold admits, and the jobs a take leaves behind.
 * Guarantees: all thirty-five claims of the original hold, with its three
 *   unasserted writes checked as well [tested: make twins; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include "thread_oracle.h"
#include <unistd.h>

static mt_atom *ref(mt_space *s) { return mt_spaceref(mt_space_name(s)); }
static mt_atom *job(int64_t n) { return E("job", n); }

/* The jobs among xs other than except, into out; how many. */
static size_t others(const int64_t *xs, size_t n, int64_t except, mt_atom **out)
{
    size_t k = 0;
    for (size_t i = 0; i < n; i++)
        if (xs[i] != except) out[k++] = job(xs[i]);
    return k;
}

/* Whether two evaluations answer the same one atom. */
static bool agree(metta *m, mt_atom *left, mt_atom *right)
{
    mt_atom *a = mt_first(mt_eval(m, left)), *b = mt_first(mt_eval(m, right));
    bool same = a && b && mt_eq(a, b);
    mt_drop(a), mt_drop(b);
    return same;
}

int main(void)
{
    metta *m = open_engine();
    import_thread_lib(m);
    publish_unary(m, "inc", inc_op, "Number");
    publish_unary(m, "big?", big_op, "Bool");

    const int64_t three[] = { 1, 2, 3 }, four[] = { 1, 2, 3, 4 }, all_big[] = { 3, 4 }, one_small[] = { 1, 4 },
                  one_big[] = { 1, 9 }, none_big[] = { 1, 2 };
    check_answers("par_map keeps the order", mt_eval(m, E("par_map", "inc", LIST(three))), mapped(three, COUNT(three), inc, NULL));
    check_answers("par_filter", mt_eval(m, E("par_filter", "big?", LIST(four))), mapped(four, COUNT(four), NULL, big));
    check_answers("par_forall", mt_eval(m, E("par_forall", "big?", LIST(all_big))), B(all_of(all_big, COUNT(all_big), big)));
    check_answers("and when one fails", mt_eval(m, E("par_forall", "big?", LIST(one_small))), B(all_of(one_small, COUNT(one_small), big)));
    check_answers("par_any", mt_eval(m, E("par_any", "big?", LIST(one_big))), B(any_of(one_big, COUNT(one_big), big)));
    check_answers("and when none holds", mt_eval(m, E("par_any", "big?", LIST(none_big))), B(any_of(none_big, COUNT(none_big), big)));
    check("par_map is par-map", agree(m, E("par_map", "inc", LIST(three)), E("par-map", "inc", LIST(three))));
    check_answers("par_race holds its branches", mt_eval(m, E("par_race", E(E("inc", 41), E("inc", 41)))), N(inc(41)));

    mt_atom *future = answered(m, "thread_spawn answers a future", E("thread_spawn", E("inc", 41)));
    check_answers("thread_await waits for it", mt_eval(m, E("thread_await", mt_keep(future))), N(inc(41)));
    check_answers("and then it is settled", mt_eval(m, E("thread_settled", future)), B(true));
    mt_atom *running = answered(m, "thread_spawn answers a future", E("thread_spawn", E("let", V("_"), E("sleep", 30), 1)));
    check_answers("a running future is not settled", mt_eval(m, E("thread_settled", mt_keep(running))), B(false));
    require("thread_cancel stops it", mt_one_truth(mt_eval(m, E("thread_cancel", running))));

    const char *word = "hi";
    mt_atom *channel = answered(m, "channel_new answers a channel", E("channel_new"));
    require("channel_send", mt_one_truth(mt_eval(m, E("channel_send", mt_keep(channel), word))));
    check_answers("channel_recv answers what was sent", mt_eval(m, E("channel_recv", mt_keep(channel))), S(word));
    require("channel_send", mt_one_truth(mt_eval(m, E("channel_send", mt_keep(channel), word))));
    check_answers("channel_size counts it", mt_eval(m, E("channel_size", mt_keep(channel))), N(1));
    mt_drop(channel);
    mt_atom *empty = answered(m, "channel_new answers a channel", E("channel_new"));
    check_none("channel_try_recv on an empty channel answers nothing", mt_eval(m, E("channel_try_recv", mt_keep(empty))));
    require("channel_send", mt_one_truth(mt_eval(m, E("channel_send", mt_keep(empty), word))));
    check_answers("and on a waiting one answers it", mt_eval(m, E("channel_try_recv", mt_keep(empty))), S(word));
    check_answers("channel_close", mt_eval(m, E("channel_close", empty)), B(true));

    const char *pool = "rung-pool";
    const int64_t size = 2;
    mt_drop(answered(m, "pool_create answers", E("pool_create", pool, size)));
    check_answers("pool_submit answers what thread_await takes", mt_eval(m, E("thread_await", E("pool_submit", pool, E("inc", 9)))), N(inc(9)));
    check_answers("pool_stats reports an idle pool", mt_eval(m, E("pool_stats", pool)), stats_atom(idle(size)));
    check_answers("pool_destroy", mt_eval(m, E("pool_destroy", pool)), B(true));

    check_answers("timer_after fires once", mt_eval(m, E("collapse", E("thread_await", E("timer_after", 0.05, E("inc", 41))))), E(N(inc(41))));
    mt_atom *repeating = answered(m, "timer_every answers a timer", E("timer_every", 0.05, E("inc", 41)));
    struct timespec let_it_fire = { 0, 200 * 1000 * 1000 };
    nanosleep(&let_it_fire, NULL);
    check_answers("timer_every repeats until thread_cancel stops it", mt_eval(m, E("thread_cancel", repeating)), B(true));
    check_answers("with_lock keeps every answer", mt_eval(m, E("collapse", E("with_lock", "rung-lock", E("superpose", LIST(three))))),
                  LIST(three));

    mt_space *jobs = mt_space_open(m, "&rung-jobs"), *work = mt_space_open(m, "&rung-work");
    require("open the two spaces", jobs && work);
    const int64_t posted = 7;
    check("a job is posted", mt_add(jobs, job(posted)));
    for (int peek = 0; peek < 2; peek++)
        check_answers("space_await leaves it", mt_eval(m, E("space_await", ref(jobs), E("job", V("n")))), job(posted));
    check_answers("space_take removes it", mt_eval(m, E("space_take", ref(jobs), E("job", V("n")))), job(posted));
    check_none("so none is left", mt_atoms(jobs));

    const int64_t queued[] = { 2, 9 }, threshold = 5, unreachable = 100;
    for (size_t i = 0; i < COUNT(queued); i++) check("a job is queued", mt_add(work, job(queued[i])));
    int64_t admitted = -1;
    for (size_t i = 0; i < COUNT(queued) && admitted < 0; i++)
        if (queued[i] > threshold) admitted = queued[i];
    mt_atom *guard = E(">", V("n"), threshold);
    check_answers("space_await_where waits for the first the guard admits", mt_eval(m, E("space_await_where", ref(work), E("job", V("n")), mt_keep(guard))),
                  job(admitted));
    check_answers("space_take_where takes it", mt_eval(m, E("space_take_where", ref(work), E("job", V("n")), guard)), job(admitted));
    mt_atom *left[COUNT(queued)];
    check_answers_("and leaves the rest", mt_atoms(work), others(queued, COUNT(queued), admitted, left), left);
    check_answers("a guard nothing meets waits out its deadline", mt_eval(m, E("collapse", E("space_take_where", ref(work), E("job", V("n")), E(">", V("n"), unreachable), 0.05))),
                  mt_unit());
    check_answers_("and every candidate it refused is still there", mt_atoms(work), others(queued, COUNT(queued), admitted, left), left);
    mt_space_close(jobs), mt_space_close(work);

    const int64_t online = (int64_t)sysconf(_SC_NPROCESSORS_ONLN);
    check_answers("cpu_count is the processors online", mt_eval(m, E("cpu_count")), N(online));
    check("thread_count counts at least this thread", mt_one_int(mt_eval(m, E("thread_count"))) >= 1);
    check("cpu_count is cpu-count", agree(m, E("cpu_count"), E("cpu-count")));
    check("thread_count is thread-count", agree(m, E("thread_count"), E("thread-count")));
    return done(m);
}
