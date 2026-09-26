/* Purpose: a parametric arrow. apply's (-> (-> $tx $ty) $tx $ty) takes a
 *   function and its argument, so applying not to False answers C's !false,
 *   and the application's type is not's own result, Bool. Unifying apply's
 *   type with (-> (-> Bool Bool) Bool $result) binds $result to Bool too.
 * Guarantees: the original's claim holds, with its two unasserted forms
 *   checked as well [tested 2026-09-27T00:35:58+10:00:
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

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("(: apply (-> (-> $tx $ty) $tx $ty))", mt_add(m, E(":", "apply", E("->", E("->", V("tx"), V("ty")), V("tx"), V("ty")))));
    require("(= (apply $f $x) ($f $x))", mt_add(m, E("=", E("apply", V("f"), V("x")), E(V("f"), V("x")))));
    assert(answers_are(mt_eval(m, E("apply", "not", B(false))), E(B(!false))) && "apply runs not");
    assert(answers_are(mt_eval(m, E("get-type", E("apply", "not", B(false)))), E(S("Bool"))) && "its type is not's result");
    assert(answers_are(mt_eval(m, E("let", E("get-type", "apply"), E("->", E("->", "Bool", "Bool"), "Bool", V("result")), V("result"))), E(S("Bool")))
           && "$result unifies to Bool");
    mt_close(m);
    return 0;
}
