/* Purpose: bindings as a value. mylet hands let* its bindings as an
 *   argument, declared Atom so the body arrives unevaluated, and C builds
 *   the bindings it hands over. A binding is a (pattern value) pair and a
 *   pattern that does not match gives no answer; noeval hands bindings to a
 *   definition that evaluates its arguments; bindings that are not pairs are
 *   refused, which C sees as MT_ERROR on the cursor; and (let* foo ok) is no
 *   binding at all but a partial application, which arrives as the
 *   expression (partial let* (foo ok)) and compares as the term C builds.
 * Guarantees: all eight claims of the original hold
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

/* (( $x 1 ) ( $y 2 )) */
static mt_atom *x_and_y(void) { return E(E(V("x"), 1), E(V("y"), 2)); }

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("(: mylet (-> Atom Atom %Undefined%))",
            mt_add(m, E(":", "mylet", E("->", "Atom", "Atom", "%Undefined%"))));
    require("mylet", mt_add(m, E("=", E("mylet", V("bindings"), V("body")), E("let*", V("bindings"), V("body")))));
    require("mylet-evaluating",
            mt_add(m, E("=", E("mylet-evaluating", V("bindings"), V("body")), E("let*", V("bindings"), V("body")))));

    assert(answers_are(mt_eval(m, E("mylet", x_and_y(), E("+", V("x"), V("y")))), E(3)) && "handed over");
    assert(answers_are(mt_eval(m, E("let*", x_and_y(), E("+", V("x"), V("y")))), E(3)) && "written out");
    assert(answers_are(mt_eval(m, E("mylet", E(E(E(V("a"), V("b")), E(1, 2))), V("b"))), E(2)) && "a binding is a pattern");
    assert(answers_are(mt_eval(m, E("mylet", E(E(5, 5)), "matched")), E("matched")) && "a pattern that matches");
    assert(!mt_first(mt_eval(m, E("mylet", E(E(5, 6)), "matched"))) && mt_ok() && "and one that does not");
    assert(answers_are(mt_eval(m, E("mylet-evaluating", E("noeval", E(E(V("x"), 1))), V("x"))), E(1))
           && "noeval carries bindings as data");

    mt_clear();
    mt_list refused = mt_all(mt_eval(m, E("mylet-evaluating", E("noeval", E(E(1, 2, 3))), "done")));
    assert(refused.len == 0 && mt_error() == MT_ERROR && "bindings that are not pairs are refused");
    mt_list_free(refused);
    mt_clear();

    assert(answers_are(mt_eval(m, E("let*", "foo", "ok")), E(E("partial", "let*", E("foo", "ok"))))
           && "no list is no bindings: a partial application");
    mt_close(m);
    return 0;
}
