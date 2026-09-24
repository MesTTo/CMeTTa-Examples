/* Purpose: lib_he's small helpers, each held against C: id answers its
 *   argument, =alpha is mt_alpha_eq, and if-equal picks its branch by mt_eq.
 * Guarantees: all four claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    require("import lib_he", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_he")))));
    require("(= (add 1 2) 3)", mt_add(m, E("=", E("add", 1, 2), 3)));
    check_answers("id", mt_eval(m, E("id", 5)), N(5));
    mt_atom *pairs[][2] = { { E("Father", V("X")), E("Father", V("Y")) }, { E("Father", V("X")), E("Son", V("X")) } };
    for (size_t i = 0; i < 2; i++) {
        check_answers(i ? "not alpha-equal" : "alpha-equal", mt_eval(m, E("=alpha", mt_keep(pairs[i][0]), mt_keep(pairs[i][1]))), B(mt_alpha_eq(pairs[i][0], pairs[i][1])));
        mt_drop(pairs[i][0]), mt_drop(pairs[i][1]);
    }
    mt_atom *one = N(1);
    check_answers("if-equal", mt_eval(m, E("if-equal", 1, 1, T("Equal"), T("Not Equal"))), T(mt_eq(one, one) ? "Equal" : "Not Equal"));
    mt_drop(one);
    return done(m);
}
