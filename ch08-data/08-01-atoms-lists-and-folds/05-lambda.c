/* Purpose: two kinds of lambda, and C's. The fake lambda is data that apply
 *   takes apart with let; the proper |-> lambda is a function value applied
 *   in place, partially, or bound by let. C's own lambda is a function
 *   pointer with its context, which mt_function makes a MeTTa value: maplist,
 *   which takes a closure built at the call site, applies one through a
 *   lambda, and the closure over k is a C function whose context holds k.
 * Guarantees: all seven claims of the original hold
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

/* (|-> ($a) (+ 1 $a)) as C: a function value with no context. */
static mt_status add_one(mt_call *call, void *user)
{
    (void)user;
    return mt_answer(call, N(1 + mt_int(mt_arg(call, 0))));
}

/* (|-> ($x $y) (42 $x $y $k)) closing over k: the context is k itself. */
static mt_status tag_with_k(mt_call *call, void *user)
{
    const int64_t *k = user;
    return mt_answer(call, E(42, mt_keep(mt_arg(call, 0)), mt_keep(mt_arg(call, 1)), *k));
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("(: apply (-> Atom %Undefined% %Undefined%))",
            mt_add(m, E(":", "apply", E("->", "Atom", "%Undefined%", "%Undefined%"))));
    require("apply", mt_add(m, E("=", E("apply", E("lambda", V("var"), V("body")), V("arg")),
                                E("eval", E("let", V("var"), V("arg"), V("body"))))));
    require("applyL1", mt_add(m, E("=", E("applyL1"), E("apply", E("lambda", V("x"), E("+", V("x"), 1)), 2))));
    require("applyL2", mt_add(m, E("=", E("applyL2"), E("apply", E("lambda", E(V("x"), V("y")), E("+", V("x"), V("y"))), E(2, 7)))));
    require("myfunc", mt_add(m, E("=", E("myfunc", V("a"), V("b")), E("cons", V("a"), V("b")))));
    require("myfunc2", mt_add(m, E("=", E("myfunc2", V("mylambda")), E(V("mylambda"), 43, 44))));

    assert(answers_are(mt_eval(m, E("applyL1")), E(3)) && "the fake lambda of one variable");
    assert(answers_are(mt_eval(m, E("applyL2")), E(9)) && "and of two");
    /* maplist is Prolog's maplist/3 and takes a closure the call site builds,
       so the C function value rides inside one. */
    assert(answers_are(mt_eval(m, E("maplist", E("|->", E(V("a")), E(mt_function(add_one, NULL, NULL), V("a"))), E(1, 2, 3))), E(E(2, 3, 4)))
           && "maplist applies a lambda around a C function value");
    assert(answers_are(mt_eval(m, E(E("|->", E(V("acc"), V("e")), E("or", E("==", 1, V("e")), V("acc"))), B(false), 1)), E(B(true)))
           && "a lambda applied where it stands");
    assert(answers_are(mt_eval(m, E("let", V("f"), E("myfunc", 42), E(E("|->", E(V("x")), E(V("f"), E(V("x"), 2, 3))), 43))), E(E(42, 43, 2, 3)))
           && "a lambda over a partial application");
    assert(answers_are(mt_eval(m, E(E(E("|->", E(V("x"), V("y")), E(42, V("x"), V("y"))), 43), 44)), E(E(42, 43, 44)))
           && "a lambda applied one argument at a time");
    static const int64_t k = 45;
    assert(answers_are(mt_eval(m, E("myfunc2", mt_function(tag_with_k, (void *)&k, NULL))), E(E(42, 43, 44, 45)))
           && "a C closure over k");
    mt_close(m);
    return 0;
}
