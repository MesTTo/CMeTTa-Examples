/* Purpose: a compile-time regression, kept as a program. evolve wraps its
 *   function argument in twice on every recursion, which once made each
 *   nested specialization build a larger key and never finish compiling;
 *   re-specializing a function already being specialized is now refused.
 *   The three equations are built as terms because the compiler is what is
 *   under test.
 * Guarantees: (evolve derive 2 stmt) terminates and answers stmt
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
    require("(= (derive $g) $g)", mt_add(m, E("=", E("derive", V("g")), V("g"))));
    require("(= (twice $r $g) ($r ($r $g)))",
            mt_add(m, E("=", E("twice", V("r"), V("g")), E(V("r"), E(V("r"), V("g"))))));
    /* (= (evolve $r $n $g) (if (== $n 0) $g (evolve (twice $r) (- $n 1) $g))) */
    require("define evolve", mt_add(m, E("=", E("evolve", V("r"), V("n"), V("g")),
        E("if", E("==", V("n"), 0), V("g"), E("evolve", E("twice", V("r")), E("-", V("n"), 1), V("g"))))));
    assert(answers_are(mt_eval(m, E("evolve", "derive", 2, "stmt")), E("stmt")) && "evolving twice terminates");
    mt_close(m);
    return 0;
}
