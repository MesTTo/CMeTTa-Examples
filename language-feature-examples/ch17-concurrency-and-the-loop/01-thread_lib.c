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
 * Guarantees: all twenty-seven claims of the original hold [tested: make
 *   twins; commit=WORKTREE].
 * Owns resources: one pthread, attached for its one write and joined.
 */
#define MT_SHORTHAND
#include "common.h"
#include "thread_oracle.h"

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
    metta *m = open_engine();
    import_thread_lib(m);
    publish_unary(m, "inc", inc_op, NULL);
    publish_unary(m, "big?", big_op, NULL);
    require("publish slow", mt_def(m, (mt_op){ .name = "slow", .arity = 1, .effect = MT_IO, .fn = slow_op }));

    const int64_t four[] = { 1, 2, 3, 4 }, five[] = { 1, 2, 3, 4, 5 }, all_big[] = { 3, 4, 5 }, one_small[] = { 1, 4, 5 },
                  late_big[] = { 1, 2, 9 }, none_big[] = { 1, 2 };
    check_answers("par-map keeps the input's order", mt_eval(m, E("par-map", "inc", LIST(four))), mapped(four, COUNT(four), inc, NULL));
    check_answers("over nothing", mt_eval(m, E("par-map", "inc", mt_unit())), mt_unit());
    check_answers("par-filter", mt_eval(m, E("par-filter", "big?", LIST(five))), mapped(five, COUNT(five), NULL, big));
    check_answers("par-forall", mt_eval(m, E("par-forall", "big?", LIST(all_big))), B(all_of(all_big, COUNT(all_big), big)));
    check_answers("and when one fails", mt_eval(m, E("par-forall", "big?", LIST(one_small))), B(all_of(one_small, COUNT(one_small), big)));
    check_answers("par-any", mt_eval(m, E("par-any", "big?", LIST(late_big))), B(any_of(late_big, COUNT(late_big), big)));
    check_answers("and when none holds", mt_eval(m, E("par-any", "big?", LIST(none_big))), B(any_of(none_big, COUNT(none_big), big)));

    check_answers("the fast branch wins", mt_eval(m, E("par-race", E(E("slow", 1), E("inc", 41)))), N(inc(41)));
    check_answers("and a failing one drops out", mt_eval(m, E("par-race", E(E("superpose", mt_unit()), E("inc", 41)))), N(inc(41)));

    mt_atom *f = answered(m, "spawn answers a future", E("spawn", E("inc", 41)));
    check_answers("a future is awaited", mt_eval(m, E("await", mt_keep(f))), N(inc(41)));
    mt_drop(f);
    mt_atom *a = answered(m, "spawn answers a future", E("spawn", E("slow", 1))), *b = answered(m, "spawn answers a future", E("spawn", E("slow", 2)));
    int64_t both = mt_one_int(mt_eval(m, E("await", mt_keep(a)))) + mt_one_int(mt_eval(m, E("await", mt_keep(b))));
    check_int("two overlap", both, 1 + 2);
    mt_drop(a), mt_drop(b);
    mt_atom *twice = answered(m, "spawn answers a future", E("spawn", E("inc", 1)));
    mt_atom *first = mt_first(mt_eval(m, E("await", mt_keep(twice))));
    check_answers("awaiting again answers the same", mt_eval(m, E("await", mt_keep(twice))), first);
    mt_drop(twice);

    const int64_t three[] = { 1, 2, 3 };
    check_answers("a future holds the whole answer set", mt_eval(m, E("collapse", E("await", answered(m, "spawn answers a future", E("spawn", E("superpose", LIST(three))))))), LIST(three));
    check_answers("the empty set too", mt_eval(m, E("collapse", E("await", answered(m, "spawn answers a future", E("spawn", E("superpose", mt_unit())))))), mt_unit());
    mt_atom *space_future = answered(m, "spawn answers a future", E("spawn", E("inc", 1)));
    check_answers("a future is a space", mt_eval(m, E("is-space", mt_keep(space_future))), B(mt_kind_of(space_future) == MT_SPACE));
    mt_drop(space_future);
    mt_atom *settled = answered(m, "spawn answers a future", E("spawn", E("inc", 41)));
    mt_drop(mt_first(mt_eval(m, E("await", mt_keep(settled)))));
    mt_space *as_space = mt_space_open(m, mt_name(settled));
    require("open the future as a space", as_space != NULL);
    check_answers("which C reads with mt_atoms", mt_atoms(as_space), N(inc(41)));
    mt_space_close(as_space);
    mt_drop(settled);

    check_answers("a timer is a future that starts later", mt_eval(m, E("collapse", E("await", E("after", 0.05, E("inc", 41))))),
                  E(N(inc(41))));
    mt_atom *pending = mt_first(mt_eval(m, E("after", 30, E("inc", 41))));
    require("after answers a timer", pending != NULL);
    check_answers("a pending timer is not settled", mt_eval(m, E("settled?", mt_keep(pending))), B(false));
    require("cancel it", mt_one_truth(mt_eval(m, E("cancel", pending))));
    mt_atom *cancelled = mt_first(mt_eval(m, E("after", 0.05, E("inc", 41))));
    require("after answers a timer", cancelled != NULL);
    require("cancel it before it fires", mt_one_truth(mt_eval(m, E("cancel", mt_keep(cancelled)))));
    struct timespec outlast = { 0, 250 * 1000 * 1000 };
    nanosleep(&outlast, NULL);
    mt_space *timer_space = mt_space_open(m, mt_name(cancelled));
    require("open the timer as a space", timer_space != NULL);
    check_none("a cancelled timer never fires", mt_atoms(timer_space));
    mt_space_close(timer_space);
    mt_drop(cancelled);

    mt_atom *channel = mt_first(mt_eval(m, E("channel")));
    require("channel answers a channel", channel != NULL);
    require("send", mt_one_truth(mt_eval(m, E("send", mt_keep(channel), "hello"))));
    check_answers("the receiver gets its own copy", mt_eval(m, E("recv", mt_keep(channel))), S("hello"));
    require("send again", mt_one_truth(mt_eval(m, E("send", mt_keep(channel), "one"))));
    check_answers("and a waiting message is counted", mt_eval(m, E("channel-size", mt_keep(channel))), N(1));
    mt_drop(channel);

    mt_atom *pool = mt_first(mt_eval(m, E("pool", "demo-pool", 2)));
    require("a pool of two", pool != NULL);
    mt_drop(pool);
    check_answers("submit answers what await takes", mt_eval(m, E("await", E("submit", "demo-pool", E("inc", 9)))), N(inc(9)));

    writer w = { .space = mt_self(m), .atom = E("ready", "now") };
    start_writer(&w);
    check_answers("await-atom blocks until another thread writes", mt_eval(m, E("await-atom", "&self", E("ready", V("what")), 10)),
                  mt_keep(w.atom));
    join_writer(&w);

    check_answers("with-lock keeps every answer", mt_eval(m, E("collapse", E("with-lock", "demo-lock", E("superpose", LIST(three))))),
                  LIST(three));
    check_answers("with_mutex answers the first", mt_eval(m, E("collapse", E("with_mutex", "demo-lock", E("superpose", LIST(three))))),
                  E(N(three[0])));
    check_answers("timeout keeps every answer", mt_eval(m, E("collapse", E("timeout", 10, E("superpose", LIST(three))))), LIST(three));
    check_answers("and a single one", mt_eval(m, E("timeout", 10, E("inc", 41))), N(inc(41)));
    return done(m);
}
