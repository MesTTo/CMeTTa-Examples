/* Purpose: an iterator whose state is a number. make-nat-iter and
 *   iter-next are C functions over that state, next answering the value and
 *   the state after it; the engine's let* threads the state through three
 *   steps, and C steps the same protocol in a loop for the values it
 *   expects.
 * Guarantees: the original's claim holds [tested 2026-09-27T00:35:58+10:00:
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

static mt_status make_nat_iter(mt_call *call, void *user)
{
    (void)user;
    return mt_answer(call, N(0));
}

/* (iter-next n): (n n+1), the value and the next state. */
static mt_status iter_next(mt_call *call, void *user)
{
    (void)user;
    int64_t n = mt_int(mt_arg(call, 0));
    return mt_answer(call, E(n, n + 1));
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("publish make-nat-iter", mt_def(m, (mt_op){ .name = "make-nat-iter", .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = make_nat_iter }));
    require("publish iter-next", mt_def(m, (mt_op){ .name = "iter-next", .arity = 1, .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = iter_next }));

    mt_atom *seen[3];
    int64_t state = 0;
    for (int i = 0; i < 3; i++) seen[i] = N(state++);
    assert(answers_are(mt_eval(m, E("let*", E(E(V("it"), E("make-nat-iter")),
                                              E(E(V("x1"), V("it1")), E("iter-next", V("it"))),
                                              E(E(V("x2"), V("it2")), E("iter-next", V("it1"))),
                                              E(E(V("x3"), V("it3")), E("iter-next", V("it2")))),
                                    E(V("x1"), V("x2"), V("x3")))), E(mt_exprv(3, seen)))
           && "three steps of the iterator");
    mt_close(m);
    return 0;
}
