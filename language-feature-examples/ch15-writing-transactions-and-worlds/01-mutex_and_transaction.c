/* Purpose: a counter five threads share, and a transaction that undoes its
 *   writes. The increment is C: it reads the one (cnt x) in &temp, removes it
 *   and adds (cnt x+1), answering the pair of the two writes' outcomes as the
 *   original's match does. mutexinc runs it under testmutex, a pthread mutex,
 *   and five pthreads, each attached to the engine, call it at once, so 37
 *   becomes 42 however they interleave; sloppyinc is the same increment
 *   without the lock, which races, and nothing calls it.
 *   Transaction_rollback_fail_to_inc runs the increment inside
 *   mt_transaction and then fails, as the original's branch ends in (empty),
 *   so the engine rolls both writes back and the call answers nothing.
 * Guarantees: both claims of the original hold, with its three unasserted
 *   forms checked as well [tested: make twins; commit=WORKTREE].
 * Owns resources: five threads, each attached to the engine for its one call
 *   and joined before the claims.
 * Guarded by: testmutex, around every read-modify-write mutexinc makes.
 */
#define MT_SHORTHAND
#include "common.h"
#include <pthread.h>

enum { INCREMENTS = 5 };

typedef struct counter {
    mt_space *temp;
    pthread_mutex_t testmutex;
} counter;

/* The (cnt x) &temp holds, or NULL. */
static mt_atom *count_atom(mt_space *temp) { return mt_first(mt_match(temp, E("cnt", V("x")))); }

static int64_t count_of(mt_space *temp)
{
    mt_atom *count = count_atom(temp);
    int64_t x = count ? mt_int(mt_at(count, 1)) : -1;
    mt_drop(count);
    return x;
}

/* The read-modify-write, one (cnt x) to (cnt x+1): the pair of the removal's
   and the addition's outcomes, or NULL when &temp holds no count or a door
   failed, which mt_ok() tells apart. */
static mt_atom *increment(mt_space *temp)
{
    mt_atom *count = count_atom(temp);
    if (!count) return NULL;
    int64_t x = mt_int(mt_at(count, 1));
    bool removed = mt_del(temp, count);
    bool added = mt_add(temp, E("cnt", x + 1));
    return mt_ok() ? E(B(removed), B(added)) : NULL;
}

static mt_status answer_increment(mt_call *call, mt_atom *pair)
{
    return pair ? mt_answer(call, pair) : mt_ok() ? MT_FAIL : mt_error();
}

static mt_status sloppyinc(mt_call *call, void *user)
{
    counter *c = user;
    return answer_increment(call, increment(c->temp));
}

static mt_status mutexinc(mt_call *call, void *user)
{
    counter *c = user;
    pthread_mutex_lock(&c->testmutex);
    mt_atom *pair = increment(c->temp);
    pthread_mutex_unlock(&c->testmutex);
    return answer_increment(call, pair);
}

/* The transaction's body: the increment lands, and then the branch fails. */
static mt_status increment_then_fail(metta *m, void *user)
{
    (void)m;
    counter *c = user;
    int64_t before = count_of(c->temp);
    mt_atom *pair = increment(c->temp);
    if (!pair) return mt_ok() ? MT_FAIL : mt_error();
    mt_drop(pair);
    check_int("inside the transaction the increment has landed", count_of(c->temp), before + 1);
    return MT_FAIL;
}

static mt_status rollback_fail_to_inc(mt_call *call, void *user)
{
    return mt_transaction(mt_of(call), increment_then_fail, user);
}

typedef struct worker {
    metta *m;
    mt_atom *answer;
} worker;

static void *run_mutexinc(void *opaque)
{
    worker *w = opaque;
    if (!mt_thread_attach()) return NULL;
    w->answer = mt_first(mt_eval(w->m, E("mutexinc")));
    mt_thread_detach();
    return NULL;
}

int main(void)
{
    metta *m = open_engine();
    counter c = { .temp = mt_space_open(m, "&temp") };
    require("open &temp", c.temp != NULL);
    require("the mutex", pthread_mutex_init(&c.testmutex, NULL) == 0);
    const mt_op ops[] = {
        { .name = "sloppyinc", .arity = 0, .effect = MT_WRITES, .fn = sloppyinc, .user = &c },
        { .name = "mutexinc", .arity = 0, .effect = MT_WRITES, .fn = mutexinc, .user = &c },
        { .name = "Transaction_rollback_fail_to_inc", .arity = 0, .effect = MT_WRITES, .fn = rollback_fail_to_inc, .user = &c },
    };
    for (size_t i = 0; i < sizeof ops / sizeof *ops; i++) require(ops[i].name, mt_def(m, ops[i]));

    const int64_t start = 37;
    check("(add-atom &temp (cnt 37)) answers True", mt_add(c.temp, E("cnt", start)));

    worker workers[INCREMENTS];
    pthread_t threads[INCREMENTS];
    for (size_t i = 0; i < INCREMENTS; i++) {
        workers[i] = (worker){ .m = m };
        require("start a thread", pthread_create(&threads[i], NULL, run_mutexinc, &workers[i]) == 0);
    }
    for (size_t i = 0; i < INCREMENTS; i++) require("join a thread", pthread_join(threads[i], NULL) == 0);
    for (size_t i = 0; i < INCREMENTS; i++)
        check_atom("each protected increment removes and adds", workers[i].answer, E(B(true), B(true)));
    check_answers("five protected increments", mt_atoms(c.temp), E("cnt", start + INCREMENTS));

    check_none("the failing transaction answers nothing", mt_eval(m, E("Transaction_rollback_fail_to_inc")));
    check_answers("and leaves the count as it was", mt_atoms(c.temp), E("cnt", start + INCREMENTS));

    mt_space_close(c.temp);
    pthread_mutex_destroy(&c.testmutex);
    return done(m);
}
