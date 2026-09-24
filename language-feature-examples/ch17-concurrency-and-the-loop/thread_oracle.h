/* Purpose: what chapter 17's lib_thread twins hold the library against: inc
 *   and big? as C functions, published under the original's names, and C's
 *   own map, filter, all and any over a C array, which each parallel
 *   collection must answer as; a pool's report as C's record of a pool, and
 *   an idle pool of a size; an evaluation that must answer; and a
 *   writer on a C pthread, attached to the engine for one write, which a
 *   blocking wait is left to meet.
 * Assumes: one runtime, opened by open_engine(), and lib_thread imported.
 */
#ifndef CH17_THREAD_ORACLE_H
#define CH17_THREAD_ORACLE_H
#include "common.h"
#include <pthread.h>
#include <time.h>

static inline int64_t inc(int64_t x) { return x + 1; }
static inline bool big(int64_t x) { return x > 2; }

static inline mt_status inc_op(mt_call *call, void *user) { (void)user; return mt_answer(call, N(inc(mt_int(mt_arg(call, 0))))); }
static inline mt_status big_op(mt_call *call, void *user) { (void)user; return mt_answer(call, B(big(mt_int(mt_arg(call, 0))))); }

static inline void import_thread_lib(metta *m)
{
    require("import lib_thread", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_thread")))));
}

/* Publish a C function of one number under the original's name, declared
   (: name (-> Number result)) when the original declares it. */
static inline void publish_unary(metta *m, const char *name, mt_fn fn, const char *result)
{
    require(name, mt_def(m, (mt_op){ .name = name, .arity = 1, .effect = MT_PURE, .fn = fn }));
    if (result) require("declare its type", mt_add(m, E(":", name, E("->", "Number", result))));
}

#define COUNT(xs) (sizeof (xs) / sizeof *(xs))

/* The expression of f over the numbers, keeping those keep admits; NULL
   for either is the identity and keep-all. */
static inline mt_atom *mapped(const int64_t *xs, size_t n, int64_t (*f)(int64_t), bool (*keep)(int64_t))
{
    mt_atom **items = malloc((n + 1) * sizeof *items);
    require("room for the list", items != NULL);
    size_t k = 0;
    for (size_t i = 0; i < n; i++)
        if (!keep || keep(xs[i])) items[k++] = N(f ? f(xs[i]) : xs[i]);
    mt_atom *out = mt_exprv(k, items);
    free(items);
    return out;
}
#define LIST(xs) mapped((xs), COUNT(xs), NULL, NULL)

static inline bool all_of(const int64_t *xs, size_t n, bool (*p)(int64_t))
{
    for (size_t i = 0; i < n; i++)
        if (!p(xs[i])) return false;
    return true;
}

static inline bool any_of(const int64_t *xs, size_t n, bool (*p)(int64_t))
{
    for (size_t i = 0; i < n; i++)
        if (p(xs[i])) return true;
    return false;
}

/* A pool's four numbers, in the order pool-stats reports them. */
enum { SIZE, RUNNING, BACKLOG, FREE, STATS };
static const char *const stat_names[STATS] = { [SIZE] = "size", [RUNNING] = "running", [BACKLOG] = "backlog", [FREE] = "free" };

typedef struct pool_stats {
    int64_t of[STATS];
} pool_stats;

static inline pool_stats idle(int64_t size) { return (pool_stats){ .of = { [SIZE] = size, [FREE] = size } }; }

static inline mt_atom *stats_atom(pool_stats s)
{
    mt_atom *items[STATS];
    for (size_t i = 0; i < STATS; i++) items[i] = E(stat_names[i], s.of[i]);
    return mt_exprv(STATS, items);
}

/* The record a pool-stats answer holds, or false for any other shape. */
static inline bool read_stats(const mt_atom *a, pool_stats *s)
{
    if (mt_kind_of(a) != MT_EXPR || mt_len(a) != STATS) return false;
    for (size_t i = 0; i < STATS; i++) {
        const mt_atom *pair = mt_at(a, i);
        if (mt_kind_of(pair) != MT_EXPR || mt_len(pair) != 2 || !mt_name(mt_at(pair, 0)) ||
            strcmp(mt_name(mt_at(pair, 0)), stat_names[i]) != 0)
            return false;
        s->of[i] = mt_int(mt_at(pair, 1));
    }
    return true;
}

/* The one answer an evaluation that must answer gives, such as a future. */
static inline mt_atom *answered(metta *m, const char *what, mt_atom *goal)
{
    mt_atom *out = mt_first(mt_eval(m, goal));
    require(what, out != NULL);
    return out;
}

/* A C thread attached to the engine for one write into a space, after a
   pause long enough that a wait started first has begun waiting. */
typedef struct writer {
    mt_space *space;
    mt_atom *atom;
    bool wrote;
    pthread_t thread;
} writer;

static inline void *write_once(void *opaque)
{
    writer *w = opaque;
    if (!mt_thread_attach()) return NULL;
    struct timespec pause = { 0, 50 * 1000 * 1000 };
    nanosleep(&pause, NULL);
    w->wrote = mt_add(w->space, mt_keep(w->atom));
    mt_thread_detach();
    return NULL;
}

static inline void start_writer(writer *w) { require("start the writer", pthread_create(&w->thread, NULL, write_once, w) == 0); }

/* Join the writer, which must have written; TAKES its atom. */
static inline void join_writer(writer *w)
{
    require("the writer wrote", pthread_join(w->thread, NULL) == 0 && w->wrote);
    mt_drop(w->atom);
}
#endif
