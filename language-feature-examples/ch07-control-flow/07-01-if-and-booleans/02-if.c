/* Purpose: the three-argument if takes the arm C's ?: takes. The same
 *   condition decides both: C builds only the arm it chooses as the
 *   expectation, and the engine gets the whole term.
 * Guarantees: the original's claim holds [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    check_answers("(if (> 1 2) (3 4) (5 6))", mt_eval(m, E("if", E(">", 1, 2), E(3, 4), E(5, 6))),
                  1 > 2 ? E(3, 4) : E(5, 6));
    return done(m);
}
