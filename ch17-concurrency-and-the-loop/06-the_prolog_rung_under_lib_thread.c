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
 *   unasserted writes checked as well [tested 2026-09-27T00:35:58+10:00:
 *   make -C extensions/cmetta corpus-check].
 */
#define _POSIX_C_SOURCE 200809L
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include "_fixtures/thread_oracle.h"
#include <unistd.h>

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
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    import_thread_lib(m);
    publish_unary(m, "inc", inc_op, "Number");
    publish_unary(m, "big?", big_op, "Bool");

    const int64_t three[] = { 1, 2, 3 }, four[] = { 1, 2, 3, 4 }, all_big[] = { 3, 4 }, one_small[] = { 1, 4 },
                  one_big[] = { 1, 9 }, none_big[] = { 1, 2 };
    assert(answers_are(mt_eval(m, E("par_map", "inc", LIST(three))), E(mapped(three, COUNT(three), inc, NULL))) && "par_map keeps the order");
    assert(answers_are(mt_eval(m, E("par_filter", "big?", LIST(four))), E(mapped(four, COUNT(four), NULL, big))) && "par_filter");
    assert(answers_are(mt_eval(m, E("par_forall", "big?", LIST(all_big))), E(B(all_of(all_big, COUNT(all_big), big)))) && "par_forall");
    assert(answers_are(mt_eval(m, E("par_forall", "big?", LIST(one_small))), E(B(all_of(one_small, COUNT(one_small), big)))) && "and when one fails");
    assert(answers_are(mt_eval(m, E("par_any", "big?", LIST(one_big))), E(B(any_of(one_big, COUNT(one_big), big)))) && "par_any");
    assert(answers_are(mt_eval(m, E("par_any", "big?", LIST(none_big))), E(B(any_of(none_big, COUNT(none_big), big)))) && "and when none holds");
    assert(agree(m, E("par_map", "inc", LIST(three)), E("par-map", "inc", LIST(three))) && "par_map is par-map");
    assert(answers_are(mt_eval(m, E("par_race", E(E("inc", 41), E("inc", 41)))), E(N(inc(41)))) && "par_race holds its branches");

    mt_atom *future = answered(m, "thread_spawn answers a future", E("thread_spawn", E("inc", 41)));
    assert(answers_are(mt_eval(m, E("thread_await", mt_keep(future))), E(N(inc(41)))) && "thread_await waits for it");
    assert(answers_are(mt_eval(m, E("thread_settled", future)), E(B(true))) && "and then it is settled");
    mt_atom *running = answered(m, "thread_spawn answers a future", E("thread_spawn", E("let", V("_"), E("sleep", 30), 1)));
    assert(answers_are(mt_eval(m, E("thread_settled", mt_keep(running))), E(B(false))) && "a running future is not settled");
    require("thread_cancel stops it", mt_one_truth(mt_eval(m, E("thread_cancel", running))));

    const char *word = "hi";
    mt_atom *channel = answered(m, "channel_new answers a channel", E("channel_new"));
    require("channel_send", mt_one_truth(mt_eval(m, E("channel_send", mt_keep(channel), word))));
    assert(answers_are(mt_eval(m, E("channel_recv", mt_keep(channel))), E(S(word))) && "channel_recv answers what was sent");
    require("channel_send", mt_one_truth(mt_eval(m, E("channel_send", mt_keep(channel), word))));
    assert(answers_are(mt_eval(m, E("channel_size", mt_keep(channel))), E(N(1))) && "channel_size counts it");
    mt_drop(channel);
    mt_atom *empty = answered(m, "channel_new answers a channel", E("channel_new"));
    assert(!mt_first(mt_eval(m, E("channel_try_recv", mt_keep(empty)))) && mt_ok() && "channel_try_recv on an empty channel answers nothing");
    require("channel_send", mt_one_truth(mt_eval(m, E("channel_send", mt_keep(empty), word))));
    assert(answers_are(mt_eval(m, E("channel_try_recv", mt_keep(empty))), E(S(word))) && "and on a waiting one answers it");
    assert(answers_are(mt_eval(m, E("channel_close", empty)), E(B(true))) && "channel_close");

    const char *pool = "rung-pool";
    const int64_t size = 2;
    mt_drop(answered(m, "pool_create answers", E("pool_create", pool, size)));
    assert(answers_are(mt_eval(m, E("thread_await", E("pool_submit", pool, E("inc", 9)))), E(N(inc(9)))) && "pool_submit answers what thread_await takes");
    assert(answers_are(mt_eval(m, E("pool_stats", pool)), E(stats_atom(idle(size)))) && "pool_stats reports an idle pool");
    assert(answers_are(mt_eval(m, E("pool_destroy", pool)), E(B(true))) && "pool_destroy");

    assert(answers_are(mt_eval(m, E("collapse", E("thread_await", E("timer_after", 0.05, E("inc", 41))))), E(E(N(inc(41))))) && "timer_after fires once");
    mt_atom *repeating = answered(m, "timer_every answers a timer", E("timer_every", 0.05, E("inc", 41)));
    struct timespec let_it_fire = { 0, 200 * 1000 * 1000 };
    nanosleep(&let_it_fire, NULL);
    assert(answers_are(mt_eval(m, E("thread_cancel", repeating)), E(B(true))) && "timer_every repeats until thread_cancel stops it");
    assert(answers_are(mt_eval(m, E("collapse", E("with_lock", "rung-lock", E("superpose", LIST(three))))), E(LIST(three)))
           && "with_lock keeps every answer");

    mt_space *jobs = mt_space_open(m, "&rung-jobs"), *work = mt_space_open(m, "&rung-work");
    require("open the two spaces", jobs && work);
    const int64_t posted = 7;
    assert(mt_add(jobs, job(posted)) && "a job is posted");
    for (int peek = 0; peek < 2; peek++)
        assert(answers_are(mt_eval(m, E("space_await", ref(jobs), E("job", V("n")))), E(job(posted))) && "space_await leaves it");
    assert(answers_are(mt_eval(m, E("space_take", ref(jobs), E("job", V("n")))), E(job(posted))) && "space_take removes it");
    assert(!mt_first(mt_atoms(jobs)) && mt_ok() && "so none is left");

    const int64_t queued[] = { 2, 9 }, threshold = 5, unreachable = 100;
    for (size_t i = 0; i < COUNT(queued); i++) assert(mt_add(work, job(queued[i])) && "a job is queued");
    int64_t admitted = -1;
    for (size_t i = 0; i < COUNT(queued) && admitted < 0; i++)
        if (queued[i] > threshold) admitted = queued[i];
    mt_atom *guard = E(">", V("n"), threshold);
    assert(answers_are(mt_eval(m, E("space_await_where", ref(work), E("job", V("n")), mt_keep(guard))), E(job(admitted)))
           && "space_await_where waits for the first the guard admits");
    assert(answers_are(mt_eval(m, E("space_take_where", ref(work), E("job", V("n")), guard)), E(job(admitted))) && "space_take_where takes it");
    mt_atom *left[COUNT(queued)];
    assert(answers_are(mt_atoms(work), mt_exprv(others(queued, COUNT(queued), admitted, left), left)) && "and leaves the rest");
    assert(answers_are(mt_eval(m, E("collapse", E("space_take_where", ref(work), E("job", V("n")), E(">", V("n"), unreachable), 0.05))), E(mt_unit()))
           && "a guard nothing meets waits out its deadline");
    assert(answers_are(mt_atoms(work), mt_exprv(others(queued, COUNT(queued), admitted, left), left)) && "and every candidate it refused is still there");
    mt_space_close(jobs), mt_space_close(work);

    const int64_t online = (int64_t)sysconf(_SC_NPROCESSORS_ONLN);
    assert(answers_are(mt_eval(m, E("cpu_count")), E(N(online))) && "cpu_count is the processors online");
    assert(mt_one_int(mt_eval(m, E("thread_count"))) >= 1 && "thread_count counts at least this thread");
    assert(agree(m, E("cpu_count"), E("cpu-count")) && "cpu_count is cpu-count");
    assert(agree(m, E("thread_count"), E("thread-count")) && "thread_count is thread-count");
    mt_close(m);
    return 0;
}
