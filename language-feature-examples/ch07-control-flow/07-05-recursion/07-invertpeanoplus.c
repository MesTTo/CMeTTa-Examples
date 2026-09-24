/* Purpose: Peano addition run every way. peano() builds a numeral with a C
 *   loop; plus is two equations; mt_solve runs it backwards for either
 *   operand, reading the unknown by name, and for both at once, where C's
 *   loop over a from 0 to 4 gives the pairs it expects; mt_first keeps the
 *   first pair, as once does.
 * Guarantees: all five claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

/* (S (S ... Z)), n deep. */
static mt_atom *peano(int n)
{
    mt_atom *numeral = S("Z");
    while (n-- > 0) numeral = E("S", numeral);
    return numeral;
}

int main(void)
{
    metta *m = open_engine();
    require("(= (plus Z $y) $y)", mt_add(m, E("=", E("plus", "Z", V("y")), V("y"))));
    require("(= (plus (S $x) $y) (S (plus $x $y)))",
            mt_add(m, E("=", E("plus", E("S", V("x")), V("y")), E("S", E("plus", V("x"), V("y"))))));

    check_answers("2 + 1 is 3", mt_eval(m, E("plus", peano(2), peano(1))), peano(3));
    check_list("A + 1 = 4 solves A", mt_all(mt_solve(m, peano(4), E("plus", V("A"), peano(1)))), peano(3));
    check_list("1 + B = 4 solves B", mt_all(mt_solve(m, peano(4), E("plus", peano(1), V("B")))), peano(3));

    mt_atom *pairs[5];
    for (int a = 0; a <= 4; a++) pairs[a] = E(peano(a), peano(4 - a));
    check_list_("A + B = 4 answers every pair", mt_all(mt_solve(m, peano(4), E("plus", V("A"), V("B")))), 5, pairs);
    check_atom("the first pair only", mt_first(mt_solve(m, peano(4), E("plus", V("A"), V("B")))), E(peano(0), peano(4)));
    return done(m);
}
