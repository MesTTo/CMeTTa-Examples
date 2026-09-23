/* Purpose: order counts. Asked before its equation is stored, a call has
 *   nothing to reduce it and answers itself; asked after, it answers what
 *   the equation says. The two asks are two C statements around one mt_add.
 * Guarantees: the call answers itself, then hello [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    check_answers("before the equation the call answers itself",
                  mt_eval(m, E("p121-example-respond", "me")), E("p121-example-respond", "me"));
    require("(= (p121-example-respond me) hello)",
            mt_add(m, E("=", E("p121-example-respond", "me"), "hello")));
    check_answers("after it, the equation answers",
                  mt_eval(m, E("p121-example-respond", "me")), "hello");
    return done(m);
}
