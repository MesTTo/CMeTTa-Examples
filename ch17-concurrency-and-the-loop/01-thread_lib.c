/* Purpose: concurrency through lib_thread, each answer held against C. inc,
 *   big? and slow are C functions, and every parallel collection is checked
 *   against C running the same function over the same C array: par-map is
 *   C's map, par-filter its filter, par-forall and par-any its all and any.
 *   slow sleeps through the engine's own sleep, which a race's stop signal
 *   interrupts, where a C sleep would hold the stopped branch to its end, so
 *   the race answers the fast branch's value. A future is held in a C
 *   variable: awaiting it answers its whole answer set, twice without waiting
 *   again, and being a space, C reads its atoms through mt_atoms. A timer is a
 *   future that starts later, cancelled before it fires, and C's own
 *   nanosleep outlasts it. A channel carries a copy, a pool bounds the
 *   fan-out, and await-atom blocks until another thread writes, here a C
 *   pthread attached to the engine. with-lock and timeout keep every answer,
 *   where with_mutex answers the first.
 * Guarantees: all twenty-seven claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
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

static const int64_t slow_seconds = 1;

/* slow: the engine's sleep, then the argument. */
static mt_status slow_op(mt_call *call, void *user)
{
    (void)user;
    mt_atom *slept = mt_first(mt_eval(mt_of(call), E("sleep", slow_seconds)));
    if (!slept) return mt_ok() ? MT_FAIL : mt_error();
    mt_drop(slept);
    return mt_answer(call, mt_keep(mt_arg(call, 0)));
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    import_thread_lib(m);
    publish_unary(m, "inc", inc_op, NULL);
    publish_unary(m, "big?", big_op, NULL);
    require("publish slow", mt_def(m, (mt_op){ .name = "slow", .arity = 1, .effect = MT_EFFECT_CLASS_ORACLE_IO, .fn = slow_op }));

    const int64_t four[] = { 1, 2, 3, 4 }, five[] = { 1, 2, 3, 4, 5 }, all_big[] = { 3, 4, 5 }, one_small[] = { 1, 4, 5 },
                  late_big[] = { 1, 2, 9 }, none_big[] = { 1, 2 };
    assert(answers_are(mt_eval(m, E("par-map", "inc", LIST(four))), E(mapped(four, COUNT(four), inc, NULL))) && "par-map keeps the input's order");
    assert(answers_are(mt_eval(m, E("par-map", "inc", mt_unit())), E(mt_unit())) && "over nothing");
    assert(answers_are(mt_eval(m, E("par-filter", "big?", LIST(five))), E(mapped(five, COUNT(five), NULL, big))) && "par-filter");
    assert(answers_are(mt_eval(m, E("par-forall", "big?", LIST(all_big))), E(B(all_of(all_big, COUNT(all_big), big)))) && "par-forall");
    assert(answers_are(mt_eval(m, E("par-forall", "big?", LIST(one_small))), E(B(all_of(one_small, COUNT(one_small), big)))) && "and when one fails");
    assert(answers_are(mt_eval(m, E("par-any", "big?", LIST(late_big))), E(B(any_of(late_big, COUNT(late_big), big)))) && "par-any");
    assert(answers_are(mt_eval(m, E("par-any", "big?", LIST(none_big))), E(B(any_of(none_big, COUNT(none_big), big)))) && "and when none holds");

    assert(answers_are(mt_eval(m, E("par-race", E(E("slow", 1), E("inc", 41)))), E(N(inc(41)))) && "the fast branch wins");
    assert(answers_are(mt_eval(m, E("par-race", E(E("superpose", mt_unit()), E("inc", 41)))), E(N(inc(41)))) && "and a failing one drops out");

    mt_atom *f = answered(m, "spawn answers a future", E("spawn", E("inc", 41)));
    assert(answers_are(mt_eval(m, E("await", mt_keep(f))), E(N(inc(41)))) && "a future is awaited");
    mt_drop(f);
    mt_atom *a = answered(m, "spawn answers a future", E("spawn", E("slow", 1))), *b = answered(m, "spawn answers a future", E("spawn", E("slow", 2)));
    int64_t both = mt_one_int(mt_eval(m, E("await", mt_keep(a)))) + mt_one_int(mt_eval(m, E("await", mt_keep(b))));
    assert(both == 1 + 2 && "two overlap");
    mt_drop(a), mt_drop(b);
    mt_atom *twice = answered(m, "spawn answers a future", E("spawn", E("inc", 1)));
    mt_atom *first = mt_first(mt_eval(m, E("await", mt_keep(twice))));
    assert(answers_are(mt_eval(m, E("await", mt_keep(twice))), E(first)) && "awaiting again answers the same");
    mt_drop(twice);

    const int64_t three[] = { 1, 2, 3 };
    assert(answers_are(mt_eval(m, E("collapse", E("await", answered(m, "spawn answers a future", E("spawn", E("superpose", LIST(three))))))), E(LIST(three))) && "a future holds the whole answer set");
    assert(answers_are(mt_eval(m, E("collapse", E("await", answered(m, "spawn answers a future", E("spawn", E("superpose", mt_unit())))))), E(mt_unit())) && "the empty set too");
    mt_atom *space_future = answered(m, "spawn answers a future", E("spawn", E("inc", 1)));
    assert(answers_are(mt_eval(m, E("is-space", mt_keep(space_future))), E(B(mt_kind_of(space_future) == MT_SPACE))) && "a future is a space");
    mt_drop(space_future);
    mt_atom *settled = answered(m, "spawn answers a future", E("spawn", E("inc", 41)));
    mt_drop(mt_first(mt_eval(m, E("await", mt_keep(settled)))));
    mt_space *as_space = mt_space_open(m, mt_name(settled));
    require("open the future as a space", as_space != NULL);
    assert(answers_are(mt_atoms(as_space), E(N(inc(41)))) && "which C reads with mt_atoms");
    mt_space_close(as_space);
    mt_drop(settled);

    assert(answers_are(mt_eval(m, E("collapse", E("await", E("after", 0.05, E("inc", 41))))), E(E(N(inc(41)))))
           && "a timer is a future that starts later");
    mt_atom *pending = mt_first(mt_eval(m, E("after", 30, E("inc", 41))));
    require("after answers a timer", pending != NULL);
    assert(answers_are(mt_eval(m, E("settled?", mt_keep(pending))), E(B(false))) && "a pending timer is not settled");
    require("cancel it", mt_one_truth(mt_eval(m, E("cancel", pending))));
    mt_atom *cancelled = mt_first(mt_eval(m, E("after", 0.05, E("inc", 41))));
    require("after answers a timer", cancelled != NULL);
    require("cancel it before it fires", mt_one_truth(mt_eval(m, E("cancel", mt_keep(cancelled)))));
    struct timespec outlast = { 0, 250 * 1000 * 1000 };
    nanosleep(&outlast, NULL);
    mt_space *timer_space = mt_space_open(m, mt_name(cancelled));
    require("open the timer as a space", timer_space != NULL);
    assert(!mt_first(mt_atoms(timer_space)) && mt_ok() && "a cancelled timer never fires");
    mt_space_close(timer_space);
    mt_drop(cancelled);

    mt_atom *channel = mt_first(mt_eval(m, E("channel")));
    require("channel answers a channel", channel != NULL);
    require("send", mt_one_truth(mt_eval(m, E("send", mt_keep(channel), "hello"))));
    assert(answers_are(mt_eval(m, E("recv", mt_keep(channel))), E(S("hello"))) && "the receiver gets its own copy");
    require("send again", mt_one_truth(mt_eval(m, E("send", mt_keep(channel), "one"))));
    assert(answers_are(mt_eval(m, E("channel-size", mt_keep(channel))), E(N(1))) && "and a waiting message is counted");
    mt_drop(channel);

    mt_atom *pool = mt_first(mt_eval(m, E("pool", "demo-pool", 2)));
    require("a pool of two", pool != NULL);
    mt_drop(pool);
    assert(answers_are(mt_eval(m, E("await", E("submit", "demo-pool", E("inc", 9)))), E(N(inc(9)))) && "submit answers what await takes");

    writer w = { .space = mt_self(m), .atom = E("ready", "now") };
    start_writer(&w);
    assert(answers_are(mt_eval(m, E("await-atom", "&self", E("ready", V("what")), 10)), E(mt_keep(w.atom)))
           && "await-atom blocks until another thread writes");
    join_writer(&w);

    assert(answers_are(mt_eval(m, E("collapse", E("with-lock", "demo-lock", E("superpose", LIST(three))))), E(LIST(three)))
           && "with-lock keeps every answer");
    assert(answers_are(mt_eval(m, E("collapse", E("with_mutex", "demo-lock", E("superpose", LIST(three))))), E(E(N(three[0]))))
           && "with_mutex answers the first");
    assert(answers_are(mt_eval(m, E("collapse", E("timeout", 10, E("superpose", LIST(three))))), E(LIST(three))) && "timeout keeps every answer");
    assert(answers_are(mt_eval(m, E("timeout", 10, E("inc", 41))), E(N(inc(41)))) && "and a single one");
    mt_close(m);
    return 0;
}
