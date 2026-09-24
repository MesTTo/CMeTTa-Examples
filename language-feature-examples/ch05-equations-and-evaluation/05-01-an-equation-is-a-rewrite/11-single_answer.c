/* Purpose: one answer, from an equation that can be edited. eval-one takes
 *   the first answer of its argument; unique-value has two equations, one of
 *   which answers nothing, so the answer is 11; noeval holds a term back;
 *   and replacing the equation that answered 11 changes the next answer.
 * Guarantees: all five claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    require("(= (unique-value) 11)", mt_add(m, E("=", E("unique-value"), 11)));
    require("(= (unique-value) (superpose ()))", mt_add(m, E("=", E("unique-value"), E("superpose", mt_unit()))));

    check_int("eval-one of (+ 1 2)", mt_one_int(mt_eval(m, E("eval-one", E("+", 1, 2)))), 3);
    check_int("a failing alternative adds no answer",
              mt_one_int(mt_eval(m, E("eval-one", E("unique-value")))), 11);
    check_answers("noeval holds the term back unreduced", mt_eval(m, E("eval-one", E("noeval", E("+", 2, 3)))),
                  E("+", 2, 3));
    check_answers("and the empty expression", mt_eval(m, E("eval-one", E("noeval", mt_unit()))), mt_unit());

    require("remove the equation that answered 11", mt_del(m, E("=", E("unique-value"), 11)));
    require("(= (unique-value) 22)", mt_add(m, E("=", E("unique-value"), 22)));
    check_int("the edit changes the next answer", mt_one_int(mt_eval(m, E("eval-one", E("unique-value")))), 22);
    return done(m);
}
