/* Purpose: collapse is mt_all. A term nothing reduces answers itself, once,
 *   so collecting its answers into a C list gives a list of one.
 * Guarantees: the original's claim holds [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    check_list("(1 2 3) answers itself, once", mt_all(mt_eval(m, E(1, 2, 3))), E(1, 2, 3));
    return done(m);
}
