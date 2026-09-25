/* Purpose: two kinds of lambda, and C's. The fake lambda is data that apply
 *   takes apart with let; the proper |-> lambda is a function value applied
 *   in place, partially, or bound by let. C's own lambda is a function
 *   pointer with its context, which mt_function makes a MeTTa value: maplist,
 *   which takes a closure built at the call site, applies one through a
 *   lambda, and the closure over k is a C function whose context holds k.
 * Guarantees: all seven claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

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
    metta *m = open_engine();
    require("(: apply (-> Atom %Undefined% %Undefined%))",
            mt_add(m, E(":", "apply", E("->", "Atom", "%Undefined%", "%Undefined%"))));
    require("apply", mt_add(m, E("=", E("apply", E("lambda", V("var"), V("body")), V("arg")),
                                E("eval", E("let", V("var"), V("arg"), V("body"))))));
    require("applyL1", mt_add(m, E("=", E("applyL1"), E("apply", E("lambda", V("x"), E("+", V("x"), 1)), 2))));
    require("applyL2", mt_add(m, E("=", E("applyL2"), E("apply", E("lambda", E(V("x"), V("y")), E("+", V("x"), V("y"))), E(2, 7)))));
    require("myfunc", mt_add(m, E("=", E("myfunc", V("a"), V("b")), E("cons", V("a"), V("b")))));
    require("myfunc2", mt_add(m, E("=", E("myfunc2", V("mylambda")), E(V("mylambda"), 43, 44))));

    check_answers("the fake lambda of one variable", mt_eval(m, E("applyL1")), 3);
    check_answers("and of two", mt_eval(m, E("applyL2")), 9);
    /* maplist is Prolog's maplist/3 and takes a closure the call site builds,
       so the C function value rides inside one. */
    check_answers("maplist applies a lambda around a C function value",
                  mt_eval(m, E("maplist", E("|->", E(V("a")), E(mt_function(add_one, NULL, NULL), V("a"))), E(1, 2, 3))),
                  E(2, 3, 4));
    check_answers("a lambda applied where it stands",
                  mt_eval(m, E(E("|->", E(V("acc"), V("e")), E("or", E("==", 1, V("e")), V("acc"))), B(false), 1)), B(true));
    check_answers("a lambda over a partial application",
                  mt_eval(m, E("let", V("f"), E("myfunc", 42), E(E("|->", E(V("x")), E(V("f"), E(V("x"), 2, 3))), 43))),
                  E(42, 43, 2, 3));
    check_answers("a lambda applied one argument at a time",
                  mt_eval(m, E(E(E("|->", E(V("x"), V("y")), E(42, V("x"), V("y"))), 43), 44)), E(42, 43, 44));
    static const int64_t k = 45;
    check_answers("a C closure over k", mt_eval(m, E("myfunc2", mt_function(tag_with_k, (void *)&k, NULL))),
                  E(42, 43, 44, 45));
    return done(m);
}
