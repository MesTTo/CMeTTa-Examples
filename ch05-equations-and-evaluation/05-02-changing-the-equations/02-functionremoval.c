/* Purpose: equations move, one at a time. f has two equations, one calling
 *   its argument and one answering 42; each is an atom C removes and puts
 *   back, and the answers follow. g, the function f is handed, is C.
 * Guarantees: (f g) answers 2 and 42, then 2, then 42, then itself
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
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

static mt_status plus_one(mt_call *call, void *user)
{
    (void)user;
    mt_clear();
    int64_t x = mt_int(mt_arg(call, 0));
    if (!mt_ok()) return mt_fail(call, "g adds one to an integer");
    return mt_answer(call, N(x + 1));
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("publish g", mt_def(m, (mt_op){ .name = "g", .arity = 1, .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = plus_one }));
    mt_atom *calls = E("=", E("f", V("g")), E(V("g"), 1));      /* (= (f $g) ($g 1)) */
    mt_atom *constant = E("=", E("f", V("g")), 42);              /* (= (f $g) 42) */
    require("the calling equation", mt_add(m, mt_keep(calls)));
    require("the constant one", mt_add(m, mt_keep(constant)));
    assert(answers_are(mt_eval(m, E("f", "g")), E(2, 42)) && "both equations answer");

    require("take the constant out", mt_del(m, mt_keep(constant)));
    assert(answers_are(mt_eval(m, E("f", "g")), E(2)) && "only the call is left");

    require("put the constant back", mt_add(m, mt_keep(constant)));
    require("take the call out", mt_del(m, calls));
    assert(answers_are(mt_eval(m, E("f", "g")), E(42)) && "only the constant is left");

    require("take the constant out again", mt_del(m, constant));
    assert(answers_are(mt_eval(m, E("f", "g")), E(E("f", "g"))) && "with no equation the call answers itself");
    mt_close(m);
    return 0;
}
