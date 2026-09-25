/* Purpose: a branch may fork. compile has one branch, a variable that
 *   matches any statement, and its value superposes two answers, so one
 *   call answers twice; C reads both off the cursor in order.
 * Guarantees: the original's claim holds [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    require("compile", mt_add(m, E("=", E("compile", V("stmt")),
                                  E("case", V("stmt"), E(E(V("stmt"), E("superpose", E("what", "what2"))))))));
    check_answers("(compile wat) answers twice", mt_eval(m, E("compile", "wat")), "what", "what2");
    return done(m);
}
