/* Purpose: what a space stores and what it only runs. Four facts go in as
 *   one batch through mt_add_all(); evaluating (bar 42), which nothing
 *   defines, answers the expression and stores nothing; and asking the space
 *   under three patterns, sorted with qsort in the engine's own order, gives
 *   back exactly the four facts. answer is a C constant.
 * Guarantees: the three matches hold the four facts and nothing else, and
 *   (answer) is 42 [tested: make twins; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

static mt_status answer(mt_call *call, void *user)
{
    (void)user;
    return mt_answer(call, N(42));
}

int main(void)
{
    metta *m = open_engine();
    mt_list facts = { mt_calloc(4, sizeof(mt_atom *)), 4 };
    require("allocate the batch", facts.items != NULL);
    facts.items[0] = E("foo", 1);
    facts.items[1] = E("foo", 2);
    facts.items[2] = E("foo", 42, 42);
    facts.items[3] = E("foo", E(42, 42));
    require("store the four facts in one batch", mt_add_all(m, facts));

    /* Nothing defines bar: each form answers itself and nothing is stored. */
    check_answers("(bar 42) answers itself", mt_eval(m, E("bar", 42)), E("bar", 42));
    check_answers("(bar 43) answers itself", mt_eval(m, E("bar", 43)), E("bar", 43));
    require("publish answer", mt_def(m, (mt_op){ .name = "answer", .arity = 0,
                                                 .effect = MT_PURE, .fn = answer }));

    /* (foo $x), (foo $x $y) and (bar $x), gathered and sorted in C. */
    mt_atom *patterns[] = { E("foo", V("x")), E("foo", V("x"), V("y")), E("bar", V("x")) };
    mt_atom *held[8];
    size_t count = 0;
    for (size_t p = 0; p < 3; p++)
        mt_each (fact, mt_match(m, patterns[p]))
            if (count < 8) held[count++] = mt_keep(fact);
    qsort(held, count, sizeof held[0], mt_order);
    mt_atom *want[] = { E("foo", 1), E("foo", 2), E("foo", 42, 42), E("foo", E(42, 42)) };
    bool same = count == 4;
    for (size_t i = 0; i < count; i++) {
        same = same && mt_alpha_eq(held[i], want[i]);
        mt_drop(held[i]);
    }
    for (size_t i = 0; i < 4; i++) mt_drop(want[i]);
    check("the space holds the four facts and no bar", same);
    check_int("(answer) is 42", mt_one_int(mt_eval(m, E("answer"))), 42);
    return done(m);
}
