/* Purpose: one constraint per argument. in holds of a member of a list,
 *   and myplus constrains its two arguments and its result with one let
 *   each, so it answers forward, refuses what falls out of range, and
 *   enumerates what reaches a value when its arguments are variables.
 * Guarantees: all six claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    /* (= (in $x $L) (let True (is-member $x $L) $x)) */
    require("define in", mt_add(m, E("=", E("in", V("x"), V("L")),
                                     E("let", B(true), E("is-member", V("x"), V("L")), V("x")))));
    /* (= (myplus $A $B) (let $A (in $X (1 2 3)) (let $B (in $Y (2 3)) (in (+ $X $Y) (3 4 5))))) */
    require("define myplus", mt_add(m, E("=", E("myplus", V("A"), V("B")),
        E("let", V("A"), E("in", V("X"), E(1, 2, 3)),
          E("let", V("B"), E("in", V("Y"), E(2, 3)),
            E("in", E("+", V("X"), V("Y")), E(3, 4, 5)))))));

    check_answers("1 + 3 is in range", mt_eval(m, E("myplus", 1, 3)), 4);
    check_none("3 + 3 is out of range", mt_eval(m, E("myplus", 3, 3)));
    check_none("4 is not an argument it takes", mt_eval(m, E("myplus", 3, 4)));
    check_answers("adding anything to 3 reaches 4 and 5", mt_eval(m, E("myplus", V("x"), 3)), 4, 5);
    check_answers("adding anything to anything", mt_eval(m, E("myplus", V("x"), V("y"))), 3, 4, 4, 5, 5);
    check_answers("which x added to 2 goes above 3",
                  mt_eval(m, E("let", B(true), E(">", E("myplus", V("x"), 2), 3), V("x"))), 2, 3);
    return done(m);
}
