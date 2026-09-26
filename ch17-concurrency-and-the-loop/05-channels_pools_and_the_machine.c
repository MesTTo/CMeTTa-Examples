/* Purpose: the rest of lib_thread's surface, each held against C. try-recv
 *   answers nothing on an empty channel, so C drains a channel with a loop
 *   that stops at the first empty receive, and what it drained is what it
 *   sent. A closed channel is gone: a second close and a receive each raise
 *   the existence error C builds for the channel it held, which the error
 *   names by a plain symbol, since a name becomes a space reference only
 *   while the engine says it is one [source: extensions/cmetta/cmetta.c,
 *   decode's is_space(); tag cmetta 2659041b3]. A pool reports
 *   four numbers, which C reads into its own record of a pool and holds
 *   against its model of an idle one, and whose running and free always sum
 *   to its size. A repeating timer is cancelled once C's own nanosleep has
 *   let it fire. cpu-count is exactly C's sysconf(_SC_NPROCESSORS_ONLN), the
 *   count SWI's cpu_count flag holds [source: swipl-devel src/os/pl-os.c,
 *   CpuCount(); tag V10.1.14], and a C pthread attached to the engine raises
 *   thread-count while it stays attached, as a spawned thread does.
 * Guarantees: all eighteen claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 * Owns resources: one pthread, attached until main releases it, then joined.
 * Guarded by: the attachment's mutex, over the state the two threads hand
 *   each other.
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

/* Whether an atom is want up to renaming variables; takes both, and shows
   them when they differ. */
static inline bool atom_is(mt_atom *got, mt_atom *want)
{
    bool holds = got && want && mt_alpha_eq(got, want);
    if (!holds) fprintf(stderr, "  got %s\n  want %s\n", got ? mt_show(got) : "nothing",
                        want ? mt_show(want) : "nothing");
    mt_drop(got);
    mt_drop(want);
    return holds;
}

/* A C thread held attached to the engine until main releases it. */
typedef struct attachment {
    pthread_mutex_t lock;
    pthread_cond_t changed;
    enum { WAITING, ATTACHED, REFUSED } state;
    bool released;
    pthread_t thread;
} attachment;

static void *stay_attached(void *opaque)
{
    attachment *a = opaque;
    bool attached = mt_thread_attach();
    pthread_mutex_lock(&a->lock);
    a->state = attached ? ATTACHED : REFUSED;
    pthread_cond_broadcast(&a->changed);
    while (attached && !a->released) pthread_cond_wait(&a->changed, &a->lock);
    pthread_mutex_unlock(&a->lock);
    if (attached) mt_thread_detach();
    return NULL;
}

static int64_t thread_count(metta *m) { return mt_one_int(mt_eval(m, E("thread-count"))); }

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    import_thread_lib(m);
    publish_unary(m, "inc", inc_op, "Number");

    mt_atom *empty = answered(m, "channel answers a channel", E("channel"));
    assert(!mt_first(mt_eval(m, E("try-recv", mt_keep(empty)))) && mt_ok() && "an empty channel's try-recv answers nothing");
    require("send", mt_one_truth(mt_eval(m, E("send", mt_keep(empty), "hello"))));
    assert(answers_are(mt_eval(m, E("try-recv", empty)), E(S("hello"))) && "and a waiting message is received");

    const char *sent[] = { "one", "two" };
    mt_atom *channel = answered(m, "channel answers a channel", E("channel"));
    mt_atom *want[COUNT(sent)], *got[COUNT(sent) + 1];
    for (size_t i = 0; i < COUNT(sent); i++) {
        require("send", mt_one_truth(mt_eval(m, E("send", mt_keep(channel), sent[i]))));
        want[i] = S(sent[i]);
    }
    size_t drained = 0;
    for (mt_atom *message; drained <= COUNT(sent) && (message = mt_first(mt_eval(m, E("try-recv", mt_keep(channel)))));)
        got[drained++] = message;
    assert(atom_is(mt_exprv(drained, got), mt_exprv(COUNT(sent), want)) && "a drain stops at the first empty receive");

    assert(answers_are(mt_eval(m, E("channel-close", mt_keep(channel))), E(B(true))) && "closing a channel");
    mt_atom *gone = E("Error", E("existence_error", "metta_channel", mt_name(channel)), V("context"));
    assert(answers_are(mt_eval(m, E("catch", E("channel-close", mt_keep(channel)))), E(mt_keep(gone)))
           && "then it is gone, so closing it again finds none");
    assert(answers_are(mt_eval(m, E("catch", E("try-recv", channel))), E(gone)) && "and so does a receive");

    const char *pool = "reporting-pool";
    const int64_t sizes[] = { 2, 1 };
    mt_drop(answered(m, "pool answers", E("pool", pool, sizes[0])));
    assert(answers_are(mt_eval(m, E("pool-stats", pool)), E(stats_atom(idle(sizes[0])))) && "a pool with nothing submitted is idle");
    pool_stats reported;
    mt_atom *stats = answered(m, "pool-stats answers", E("pool-stats", pool));
    require("the stats read as a pool's record", read_stats(stats, &reported));
    mt_drop(stats);
    assert(reported.of[RUNNING] + reported.of[FREE] == reported.of[SIZE] && "running and free sum to the size");
    assert(answers_are(mt_eval(m, E("await", E("submit", pool, E("inc", 9)))), E(N(inc(9)))) && "a submitted job answers through await");
    assert(answers_are(mt_eval(m, E("pool-stats", pool)), E(stats_atom(idle(sizes[0])))) && "and the pool is idle again");
    assert(answers_are(mt_eval(m, E("pool-destroy", pool)), E(B(true))) && "pool-destroy frees the workers");
    mt_drop(answered(m, "pool answers", E("pool", pool, sizes[1])));
    assert(answers_are(mt_eval(m, E("pool-stats", pool)), E(stats_atom(idle(sizes[1])))) && "so the name serves a pool of another size");
    assert(answers_are(mt_eval(m, E("pool-destroy", pool)), E(B(true))) && "which is destroyed in turn");

    mt_atom *repeating = answered(m, "every answers a timer", E("every", 0.05, E("inc", 41)));
    struct timespec let_it_fire = { 0, 200 * 1000 * 1000 };
    nanosleep(&let_it_fire, NULL);
    assert(answers_are(mt_eval(m, E("cancel", repeating)), E(B(true))) && "a repeating timer is cancelled like a single one");
    mt_atom *pending = answered(m, "every answers a timer", E("every", 30, E("inc", 41)));
    assert(answers_are(mt_eval(m, E("settled?", mt_keep(pending))), E(B(false))) && "and one not yet fired is not settled");
    require("cancel it", mt_one_truth(mt_eval(m, E("cancel", pending))));

    assert(answers_are(mt_eval(m, E("cpu-count")), E(N((int64_t)sysconf(_SC_NPROCESSORS_ONLN)))) && "cpu-count is the processors online");
    int64_t before = thread_count(m);
    assert(before >= 1 && "thread-count counts at least this thread");
    attachment a = { .lock = PTHREAD_MUTEX_INITIALIZER, .changed = PTHREAD_COND_INITIALIZER, .state = WAITING };
    require("start the attaching thread", pthread_create(&a.thread, NULL, stay_attached, &a) == 0);
    pthread_mutex_lock(&a.lock);
    while (a.state == WAITING) pthread_cond_wait(&a.changed, &a.lock);
    pthread_mutex_unlock(&a.lock);
    require("the thread attached", a.state == ATTACHED);
    int64_t during = thread_count(m);
    pthread_mutex_lock(&a.lock);
    a.released = true;
    pthread_cond_broadcast(&a.changed);
    pthread_mutex_unlock(&a.lock);
    require("join the attached thread", pthread_join(a.thread, NULL) == 0);
    assert(during > before && "an attached C thread raises thread-count while it lives");
    mt_close(m);
    return 0;
}
