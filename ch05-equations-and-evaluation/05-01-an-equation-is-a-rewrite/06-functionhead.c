/* Purpose: an argument constrained to be what a call produces. myfunc is
 *   built as an equation because it must run backwards: let unifies h's
 *   argument with (myfunc (10) $B), so append runs in reverse and $B comes
 *   out bound. h_old spells the same constraint with = inside an if. Both
 *   are equations C builds as terms; no C function can run backwards.
 * Guarantees: both answer ((40) 42000) for (42 10 40)
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

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    /* (= (myfunc $A $B) (append (append (42) $A) $B)) */
    require("define myfunc", mt_add(m, E("=", E("myfunc", V("A"), V("B")),
                                         E("append", E("append", E(42), V("A")), V("B")))));
    /* (= (h_old $A $C) (if (= $A (myfunc (10) $B)) ($B $C) (empty))) */
    require("define h_old", mt_add(m, E("=", E("h_old", V("A"), V("C")),
        E("if", E("=", V("A"), E("myfunc", E(10), V("B"))), E(V("B"), V("C")), E("empty")))));
    /* (= (h $A $C) (let $A (myfunc (10) $B) ($B $C))) */
    require("define h", mt_add(m, E("=", E("h", V("A"), V("C")),
        E("let", V("A"), E("myfunc", E(10), V("B")), E(V("B"), V("C"))))));

    assert(answers_are(mt_eval(m, E("h", E(42, 10, 40), 42000)), E(E(E(40), 42000)))
           && "let runs myfunc backwards");
    assert(answers_are(mt_eval(m, E("h_old", E(42, 10, 40), 42000)), E(E(E(40), 42000)))
           && "and so does = inside if");
    mt_close(m);
    return 0;
}
