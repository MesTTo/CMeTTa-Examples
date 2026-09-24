/* Purpose: a symbol is not a variable. is-var asks the engine what
 *   mt_kind_of asks in C, and the arm C picks from its own answer is the
 *   engine's answer.
 * Guarantees: the original's claim holds [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    mt_atom *a = S("a");
    check_answers("(if (is-var a) (() (+ 1 1)) (+ 2 2))",
                  mt_eval(m, E("if", E("is-var", mt_keep(a)), E(mt_unit(), E("+", 1, 1)), E("+", 2, 2))),
                  mt_kind_of(a) == MT_VARIABLE ? E(mt_unit(), 1 + 1) : N(2 + 2));
    mt_drop(a);
    return done(m);
}
