/* Purpose: lets and superpositions stacked four ways, each equation built as
 *   an atom, then collapsed together. program1 collapses 12 and its argument plus 4,
 *   program2 fans out a collapsed list, program3 branches into (42 43), and
 *   program4 gathers the three calls: the three fan-outs of program2 each
 *   carry the other two answers. C builds the expected rows in a loop.
 * Guarantees: the original's claim holds [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    require("program1", mt_add(m, E("=", E("program1", V("Y")),
                                   E("let", V("X"), V("Y"), E("collapse", E("superpose", E(12, E("+", V("X"), 4))))))));
    require("program2", mt_add(m, E("=", E("program2", V("Y")),
                                   E("let", V("list"), E("let", V("L"), E(1, 2, 3), E("collapse", E("superpose", V("L")))),
                                     E("superpose", V("list"))))));
    require("program3", mt_add(m, E("=", E("program3", V("x")),
                                   E("if", E("==", V("x"), 2),
                                     E("let", V("z"), E("superpose", E(E("if", E("<", V("x"), 10), E("superpose", E(E(42, 43))), 43))), V("z")),
                                     E("let", V("z"), 4, V("z"))))));
    require("program4", mt_add(m, E("=", E("program4"), E("collapse", E(E("program1", 42), E("program2", 42), E("program3", 2))))));

    mt_atom *rows[3];
    for (int64_t n = 1; n <= 3; n++) rows[n - 1] = E(E(12, 42 + 4), n, E(42, 43));
    check_answers("(program4)", mt_eval(m, E("program4")), mt_exprv(3, rows));
    return done(m);
}
