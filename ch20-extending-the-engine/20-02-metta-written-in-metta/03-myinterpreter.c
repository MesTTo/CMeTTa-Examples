/* Purpose: an interpreter in three lines. myinterpreter's parameter is
 *   typed Atom, so it receives the code it is handed unreduced, announces
 *   it and then evaluates it. The code is BRANCH, one body over the C_ and
 *   T_ operators: built with the atom builders it is the (if ...) term handed
 *   over as data, and compiled with C's operators it is the choice C makes
 *   between w's and v's constants, which the interpreter must answer.
 * Guarantees: both claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>

/* A door the program needs before it can go on: on refusal, say why and stop. */
#define require(what, ok) \
    ((ok) ? (void)0 : (fprintf(stderr, "%s: %s\n", (what), mt_errmsg()), exit(EXIT_FAILURE)))

/* Each MeTTa operator twice, for a body written once over its operators:
   C_X computes in C, and T_X builds the atom (X a b). */
#define C_IF(c, t, e) ((c) ? (t) : (e))
#define T_IF(c, t, e) mt_expr("if", c, t, e)
#define C_EQ(a, b) ((a) == (b))
#define T_EQ(a, b) mt_expr("==", a, b)

#define BRANCH(IF, EQ, a, b, then, otherwise) IF(EQ(a, b), then, otherwise)

enum { W_VALUE = 42, V_VALUE = 43 };

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("(: myinterpreter (-> Atom %Undefined%))", mt_add(m, E(":", "myinterpreter", E("->", "Atom", "%Undefined%"))));
    require("myinterpreter",
            mt_add(m, E("=", E("myinterpreter", V("code")),
                        E("let", V("temp"), E("println!", E(T("Runtime-interpreting code"), V("code"))), E("eval", V("code"))))));
    require("(= (w) 42)", mt_add(m, E("=", E("w"), W_VALUE)));
    require("(= (v) 43)", mt_add(m, E("=", E("v"), V_VALUE)));

    static const int64_t compared[][2] = { { 1, 1 }, { 1, 2 } };
    for (size_t i = 0; i < sizeof compared / sizeof *compared; i++) {
        int64_t a = compared[i][0], b = compared[i][1];
        assert(mt_one_int(mt_eval(m, E("myinterpreter", BRANCH(T_IF, T_EQ, N(a), N(b), E("w"), E("v"))))) == BRANCH(C_IF, C_EQ, a, b, W_VALUE, V_VALUE)
               && "the interpreter evaluates the code it was handed");
    }
    mt_close(m);
    return 0;
}
