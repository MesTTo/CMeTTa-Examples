/* Purpose: comments are the reader's business and never reach a term. C has
 *   its own, so they sit where the original puts its MeTTa ones, inside the
 *   constructor call that builds (= (f) 42), and the engine sees the same
 *   equation the original's commented source reads to.
 * Guarantees: (f) is 42 [tested 2026-09-27T00:35:58+10:00:
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

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    /* let's comment */
    require("define f", mt_add(m, E("=", E("f"), /* with a comment */
                                   /* this is a line with just a comment */
                                   42)));  /* overall we tested systematically several comments */

    assert(mt_one_int(mt_eval(m, E("f"))) == 42 && "(f) is 42");  /* and added an evil comment for fun */
    /* anything else to comment? */
    mt_close(m);
    return 0;
}
