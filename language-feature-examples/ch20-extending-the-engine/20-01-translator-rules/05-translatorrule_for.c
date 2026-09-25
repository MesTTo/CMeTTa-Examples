/* Purpose: a for loop the language did not have, added as a translator
 *   rule. for is an equation whose body is the let over a superposition it
 *   expands into, built as the term it is; once it is a rule, a definition
 *   written with it compiles to that expansion. myfun's body is CLASSIFY,
 *   one body over lowering.h's operators: over their atom builders it is the
 *   loop's body in MeTTa, and over C's operators it is the loop body of the C for loop that
 *   says what the engine must answer, (odd 3) then (even 4).
 * Guarantees: the original's claim holds [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include "lowering.h"

#define CLASSIFY(IF, EQ, MOD, EVEN, ODD, x) IF(EQ(MOD(x, 2), 0), EVEN(x), ODD(x))
#define T_EVEN(x) E("even", x)
#define T_ODD(x) E("odd", x)

static mt_atom *even(int64_t x) { return E("even", x); }
static mt_atom *odd(int64_t x) { return E("odd", x); }

static const int64_t items[] = { 3, 4 };
#define ITEMS (sizeof items / sizeof *items)

int main(void)
{
    metta *m = open_engine();
    require("(: for (-> Atom Atom Atom %Undefined%))", mt_add(m, E(":", "for", E("->", "Atom", "Atom", "Atom", "%Undefined%"))));
    require("for's expansion",
            mt_add(m, E("=", E("for", V("var"), V("collection"), V("body")),
                        E("noeval", E("let", V("var"), E("superpose", V("collection")), V("body"))))));
    require("for is a rule", mt_one_truth(mt_eval(m, E("add-translator-rule!", "for"))));
    require("myfun loops with it", mt_add(m, E("=", E("myfun", V("L")),
                                              E("for", V("x"), V("L"), CLASSIFY(T_IF, T_EQ, T_MOD, T_EVEN, T_ODD, V("x"))))));

    mt_atom *list[ITEMS], *want[ITEMS];
    for (size_t i = 0; i < ITEMS; i++) {
        list[i] = N(items[i]);
        want[i] = CLASSIFY(C_IF, C_EQ, C_MOD, even, odd, items[i]);
    }
    check_answers_("each item classified in order", mt_eval(m, E("myfun", mt_exprv(ITEMS, list))), ITEMS, want);
    return done(m);
}
