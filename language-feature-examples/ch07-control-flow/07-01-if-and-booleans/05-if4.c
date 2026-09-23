/* Purpose: an if inside a condition. The engine decides the condition with
 *   one if and takes an arm with another; C nests ?: the same way over the
 *   same comparison, and both land on 42.
 * Guarantees: the original's claim holds [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    check_answers("(if (if (== 42 42) True False) (if True 42 lol) (+ 2 2))",
                  mt_eval(m, E("if", E("if", E("==", 42, 42), B(true), B(false)),
                               E("if", B(true), 42, "lol"), E("+", 2, 2))),
                  (42 == 42 ? true : false) ? (true ? N(42) : S("lol")) : N(2 + 2));
    return done(m);
}
