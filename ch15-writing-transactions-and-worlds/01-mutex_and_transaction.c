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
 *   forms checked as well [tested 2026-09-27T00:35:58+10:00:
 *   make -C extensions/cmetta corpus-check].
 * Owns resources: five threads, each attached to the engine for its one call
 *   and joined before the claims.
 * Guarded by: testmutex, around every read-modify-write mutexinc makes.
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

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
    assert(count_of(c->temp) == before + 1 && "inside the transaction the increment has landed");
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
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    counter c = { .temp = mt_space_open(m, "&temp") };
    require("open &temp", c.temp != NULL);
    require("the mutex", pthread_mutex_init(&c.testmutex, NULL) == 0);
    const mt_op ops[] = {
        { .name = "sloppyinc", .arity = 0, .effect = MT_EFFECT_CLASS_WRITES_STATE, .fn = sloppyinc, .user = &c },
        { .name = "mutexinc", .arity = 0, .effect = MT_EFFECT_CLASS_WRITES_STATE, .fn = mutexinc, .user = &c },
        { .name = "Transaction_rollback_fail_to_inc", .arity = 0, .effect = MT_EFFECT_CLASS_WRITES_STATE, .fn = rollback_fail_to_inc, .user = &c },
    };
    for (size_t i = 0; i < sizeof ops / sizeof *ops; i++) require(ops[i].name, mt_def(m, ops[i]));

    const int64_t start = 37;
    assert(mt_add(c.temp, E("cnt", start)) && "(add-atom &temp (cnt 37)) answers True");

    worker workers[INCREMENTS];
    pthread_t threads[INCREMENTS];
    for (size_t i = 0; i < INCREMENTS; i++) {
        workers[i] = (worker){ .m = m };
        require("start a thread", pthread_create(&threads[i], NULL, run_mutexinc, &workers[i]) == 0);
    }
    for (size_t i = 0; i < INCREMENTS; i++) require("join a thread", pthread_join(threads[i], NULL) == 0);
    for (size_t i = 0; i < INCREMENTS; i++)
        assert(atom_is(workers[i].answer, E(B(true), B(true))) && "each protected increment removes and adds");
    assert(answers_are(mt_atoms(c.temp), E(E("cnt", start + INCREMENTS))) && "five protected increments");

    assert(!mt_first(mt_eval(m, E("Transaction_rollback_fail_to_inc"))) && mt_ok() && "the failing transaction answers nothing");
    assert(answers_are(mt_atoms(c.temp), E(E("cnt", start + INCREMENTS))) && "and leaves the count as it was");

    mt_space_close(c.temp);
    pthread_mutex_destroy(&c.testmutex);
    mt_close(m);
    return 0;
}
