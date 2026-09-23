/* Purpose: two ways to take one match, and C's own. The original's two
 *   equations stop a match after its first answer with cut and with once;
 *   they are built as terms, since their bodies are MeTTa control. In C the
 *   same thing is mt_first(), which takes the first answer and closes the
 *   cursor with the rest uncomputed.
 * Guarantees: both equations answer only (a b), and so does mt_first()
 *   [tested: make twins; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
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

    check_answers("cut stops at the first match",
                  mt_eval(m, E("match-single-via-cut", "&self", E("a", V("x")), E("a", V("x")))),
                  E("a", "b"));
    check_answers("so does once",
                  mt_eval(m, E("match-single-via-once", "&self", E("a", V("x")), E("a", V("x")))),
                  E("a", "b"));
    check_atom("and so does mt_first()", mt_first(mt_match(m, E("a", V("x")))), E("a", "b"));
    return done(m);
}
