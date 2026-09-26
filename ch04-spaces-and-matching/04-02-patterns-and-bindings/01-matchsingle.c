/* Purpose: two ways to take one match, and C's own. The original's two
 *   equations stop a match after its first answer with cut and with once;
 *   they are built as terms, since their bodies are MeTTa control. In C the
 *   same thing is mt_first(), which takes the first answer and closes the
 *   cursor with the rest uncomputed.
 * Guarantees: both equations answer only (a b), and so does mt_first()
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

/* Whether an atom is want up to renaming variables; takes both, and shows
   them when they differ. */
static inline bool atom_is(mt_atom *got, mt_atom *want)
{
    bool holds = got && want && mt_alpha_eq(got, want);
    if (!holds) fprintf(stderr, "  got %s\n  want %s\n", got ? mt_show(got) : "nothing",
                        want ? mt_show(want) : "nothing");
    mt_drop(got);
    mt_drop(want);
    return holds;
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("(a b)", mt_add(m, E("a", "b")));
    require("(a c)", mt_add(m, E("a", "c")));

    /* (= (match-single-via-cut $space $pattern $out)
          (let* (($x (match $space $pattern $out)) ($temp (cut))) $x)) */
    require("define the cut version", mt_add(m, E("=",
        E("match-single-via-cut", V("space"), V("pattern"), V("out")),
        E("let*", E(E(V("x"), E("match", V("space"), V("pattern"), V("out"))),
                    E(V("temp"), E("cut"))),
          V("x")))));
    /* (= (match-single-via-once $space $pattern $out) (once (match $space $pattern $out))) */
    require("define the once version", mt_add(m, E("=",
        E("match-single-via-once", V("space"), V("pattern"), V("out")),
        E("once", E("match", V("space"), V("pattern"), V("out"))))));

    assert(answers_are(mt_eval(m, E("match-single-via-cut", "&self", E("a", V("x")), E("a", V("x")))), E(E("a", "b")))
           && "cut stops at the first match");
    assert(answers_are(mt_eval(m, E("match-single-via-once", "&self", E("a", V("x")), E("a", V("x")))), E(E("a", "b")))
           && "so does once");
    assert(atom_is(mt_first(mt_match(m, E("a", V("x")))), E("a", "b")) && "and so does mt_first()");
    mt_close(m);
    return 0;
}
