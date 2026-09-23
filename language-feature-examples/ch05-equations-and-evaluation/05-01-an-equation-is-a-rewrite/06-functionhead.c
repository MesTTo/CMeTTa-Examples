/* Purpose: an argument constrained to be what a call produces. myfunc is
 *   built as an equation because it must run backwards: let unifies h's
 *   argument with (myfunc (10) $B), so append runs in reverse and $B comes
 *   out bound. h_old spells the same constraint with = inside an if. Both
 *   are equations C builds as terms; no C function can run backwards.
 * Guarantees: both answer ((40) 42000) for (42 10 40) [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    /* (= (myfunc $A $B) (append (append (42) $A) $B)) */
    require("define myfunc", mt_add(m, E("=", E("myfunc", V("A"), V("B")),
                                         E("append", E("append", E(42), V("A")), V("B")))));
    /* (= (h_old $A $C) (if (= $A (myfunc (10) $B)) ($B $C) (empty))) */
    require("define h_old", mt_add(m, E("=", E("h_old", V("A"), V("C")),
        E("if", E("=", V("A"), E("myfunc", E(10), V("B"))), E(V("B"), V("C")), E("empty")))));
    /* (= (h $A $C) (let $A (myfunc (10) $B) ($B $C))) */
    require("define h", mt_add(m, E("=", E("h", V("A"), V("C")),
        E("let", V("A"), E("myfunc", E(10), V("B")), E(V("B"), V("C"))))));

    check_answers("let runs myfunc backwards", mt_eval(m, E("h", E(42, 10, 40), 42000)),
                  E(E(40), 42000));
    check_answers("and so does = inside if", mt_eval(m, E("h_old", E(42, 10, 40), 42000)),
                  E(E(40), 42000));
    return done(m);
}
