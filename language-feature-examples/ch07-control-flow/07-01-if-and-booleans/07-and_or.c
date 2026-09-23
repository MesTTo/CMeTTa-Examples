/* Purpose: the boolean connectives. The engine reduces (or (and true false)
 *   true) and takes an arm; C's && and || over the same values pick the
 *   expected arm.
 * Guarantees: the original's claim holds [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    check_answers("(if (or (and true false) true) 1 2)",
                  mt_eval(m, E("if", E("or", E("and", B(true), B(false)), B(true)), 1, 2)),
                  (true && false) || true ? 1 : 2);
    return done(m);
}
