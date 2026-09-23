/* Purpose: comments are the reader's business and never reach a term. C has
 *   its own, so they sit where the original puts its MeTTa ones, inside the
 *   constructor call that builds (= (f) 42), and the engine sees the same
 *   equation the original's commented source reads to.
 * Guarantees: (f) is 42 [tested: make twins; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    /* let's comment */
    require("define f", mt_add(m, E("=", E("f"), /* with a comment */
                                   /* this is a line with just a comment */
                                   42)));  /* overall we tested systematically several comments */

    check_int("(f) is 42", mt_one_int(mt_eval(m, E("f"))), 42);  /* and added an evil comment for fun */
    /* anything else to comment? */
    return done(m);
}
