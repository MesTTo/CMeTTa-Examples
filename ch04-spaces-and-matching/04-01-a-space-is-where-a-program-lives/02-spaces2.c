/* Purpose: what a space stores and what it only runs. Four facts go in as
 *   one batch through mt_add_all(); evaluating (bar 42), which nothing
 *   defines, answers the expression and stores nothing; and asking the space
 *   under three patterns, sorted with qsort in the engine's own order, gives
 *   back exactly the four facts. answer is a C constant.
 * Guarantees: the three matches hold the four facts and nothing else, and
 *   (answer) is 42 [tested 2026-09-27T00:35:58+10:00:
 *   make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>

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

static mt_status answer(mt_call *call, void *user)
{
    (void)user;
    return mt_answer(call, N(42));
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    mt_list facts = { mt_calloc(4, sizeof(mt_atom *)), 4 };
    require("allocate the batch", facts.items != NULL);
    facts.items[0] = E("foo", 1);
    facts.items[1] = E("foo", 2);
    facts.items[2] = E("foo", 42, 42);
    facts.items[3] = E("foo", E(42, 42));
    require("store the four facts in one batch", mt_add_all(m, facts));

    /* Nothing defines bar: each form answers itself and nothing is stored. */
    assert(answers_are(mt_eval(m, E("bar", 42)), E(E("bar", 42))) && "(bar 42) answers itself");
    assert(answers_are(mt_eval(m, E("bar", 43)), E(E("bar", 43))) && "(bar 43) answers itself");
    require("publish answer", mt_def(m, (mt_op){ .name = "answer", .arity = 0,
                                                 .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = answer }));

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
    assert(same && "the space holds the four facts and no bar");
    assert(mt_one_int(mt_eval(m, E("answer"))) == 42 && "(answer) is 42");
    mt_close(m);
    return 0;
}
