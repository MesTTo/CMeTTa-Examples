/* Purpose: a program editing itself. function1's equation is an atom C
 *   builds, removes and replaces, and each call answers from whatever
 *   equation the space holds at that moment.
 * Guarantees: with no equation (function1) answers itself, and after the new
 *   one it answers (OK) [tested: make twins; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    require("(= (function1) OK)", mt_add(m, E("=", E("function1"), "OK")));
    require("remove it again", mt_del(m, E("=", E("function1"), "OK")));
    check_answers("with no equation the call answers itself",
                  mt_eval(m, E("function1")), E("function1"));
    check_answers("so repr prints it as written",
                  mt_eval(m, E("repr", E("function1"))), T("(function1)"));

    require("(= (function1) (OK))", mt_add(m, E("=", E("function1"), E("OK"))));
    check_answers("the new equation answers", mt_eval(m, E("function1")), E("OK"));
    check_answers("which prints as (OK)", mt_eval(m, E("repr", E("function1"))), T("(OK)"));
    return done(m);
}
