/* Purpose: an interpreter in three lines. myinterpreter's parameter is
 *   typed Atom, so it receives the code it is handed unreduced, announces
 *   it and then evaluates it. The code is BRANCH, one body over lowering.h's
 *   operators: built with the atom builders it is the (if ...) term handed
 *   over as data, and compiled with C's operators it is the choice C makes
 *   between w's and v's constants, which the interpreter must answer.
 * Guarantees: both claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include "lowering.h"

#define BRANCH(IF, EQ, a, b, then, otherwise) IF(EQ(a, b), then, otherwise)

enum { W_VALUE = 42, V_VALUE = 43 };

int main(void)
{
    metta *m = open_engine();
    require("(: myinterpreter (-> Atom %Undefined%))", mt_add(m, E(":", "myinterpreter", E("->", "Atom", "%Undefined%"))));
    require("myinterpreter",
            mt_add(m, E("=", E("myinterpreter", V("code")),
                        E("let", V("temp"), E("println!", E(T("Runtime-interpreting code"), V("code"))), E("eval", V("code"))))));
    require("(= (w) 42)", mt_add(m, E("=", E("w"), W_VALUE)));
    require("(= (v) 43)", mt_add(m, E("=", E("v"), V_VALUE)));

    static const int64_t compared[][2] = { { 1, 1 }, { 1, 2 } };
    for (size_t i = 0; i < sizeof compared / sizeof *compared; i++) {
        int64_t a = compared[i][0], b = compared[i][1];
        check_int("the interpreter evaluates the code it was handed",
                  mt_one_int(mt_eval(m, E("myinterpreter", BRANCH(T_IF, T_EQ, N(a), N(b), E("w"), E("v"))))),
                  BRANCH(C_IF, C_EQ, a, b, W_VALUE, V_VALUE));
    }
    return done(m);
}
