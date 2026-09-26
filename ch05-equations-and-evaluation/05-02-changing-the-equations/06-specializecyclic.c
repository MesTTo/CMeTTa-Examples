/* Purpose: specialization through a cycle. f1 and f2 call each other with
 *   their function argument, as do f3 and f4, and the specializer follows the
 *   cycle without looping; the branch that would call the function never
 *   runs.
 * Guarantees: (f1 + 2) and (f3 + 1) both answer finish
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
    mt_atom *never = E(V("f"), "nevercalled", 42);           /* ($f nevercalled 42) */
    /* (= (f1 $f $a) (if (< $a 0) ($f nevercalled 42) (if (== $a 0) (f2 $f (- $a 1)) finish))) */
    require("define f1", mt_add(m, E("=", E("f1", V("f"), V("a")),
        E("if", E("<", V("a"), 0), mt_keep(never),
          E("if", E("==", V("a"), 0), E("f2", V("f"), E("-", V("a"), 1)), "finish")))));
    /* (= (f2 $f $a) (if (< $a 0) ($f nevercalled 42) (f1 $f $a))) */
    require("define f2", mt_add(m, E("=", E("f2", V("f"), V("a")),
        E("if", E("<", V("a"), 0), never, E("f1", V("f"), V("a"))))));
    assert(answers_are(mt_eval(m, E("f1", "+", 2)), E("finish")) && "the first cycle");

    /* (= (f3 $f $n) (if (== $n 0) finish (f4 $f $n))) and (= (f4 $f $n) (f3 $f (- $n 1))) */
    require("define f3", mt_add(m, E("=", E("f3", V("f"), V("n")),
                                     E("if", E("==", V("n"), 0), "finish", E("f4", V("f"), V("n"))))));
    require("define f4", mt_add(m, E("=", E("f4", V("f"), V("n")), E("f3", V("f"), E("-", V("n"), 1)))));
    assert(answers_are(mt_eval(m, E("f3", "+", 1)), E("finish")) && "the second cycle");
    mt_close(m);
    return 0;
}
