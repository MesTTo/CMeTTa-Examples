/* Purpose: an equation added as an atom and evaluated under a branch budget
 *   the program states. The equation is fib.h's FIB body built as the atom it
 *   is and added through the space's own door, and its value at 30 is fib.h's
 *   C function, the same body run by C.
 * Guarantees: the original's claim holds [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"
#include "../../ch07-control-flow/07-05-recursion/fib.h"

int main(void)
{
    metta *m = open_engine();
    require("add the equation", mt_add(m, fib_equation()));
    check_answers("fib 30 under the budget", mt_eval(m, E("with-pragma!", E(E("max-stack-depth", 100000000)), E("fib", 30))), fib(30));
    return done(m);
}
