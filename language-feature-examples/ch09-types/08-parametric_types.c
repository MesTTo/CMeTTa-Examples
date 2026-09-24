/* Purpose: a parametric arrow. apply's (-> (-> $tx $ty) $tx $ty) takes a
 *   function and its argument, so applying not to False answers C's !false,
 *   and the application's type is not's own result, Bool. Unifying apply's
 *   type with (-> (-> Bool Bool) Bool $result) binds $result to Bool too.
 * Guarantees: the original's claim holds, with its two unasserted forms
 *   checked as well [tested: make twins; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    require("(: apply (-> (-> $tx $ty) $tx $ty))", mt_add(m, E(":", "apply", E("->", E("->", V("tx"), V("ty")), V("tx"), V("ty")))));
    require("(= (apply $f $x) ($f $x))", mt_add(m, E("=", E("apply", V("f"), V("x")), E(V("f"), V("x")))));
    check_answers("apply runs not", mt_eval(m, E("apply", "not", B(false))), B(!false));
    check_answers("its type is not's result", mt_eval(m, E("get-type", E("apply", "not", B(false)))), S("Bool"));
    check_answers("$result unifies to Bool",
                  mt_eval(m, E("let", E("get-type", "apply"), E("->", E("->", "Bool", "Bool"), "Bool", V("result")), V("result"))), S("Bool"));
    return done(m);
}
