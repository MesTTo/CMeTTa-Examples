/* Purpose: a declared constructor is sorted data. SortedPoint's arrow has no
 *   equation, so an application is a point whose type is the arrow's last
 *   type, and methods destructure it. C takes the same fields: x is the
 *   first, the norm is its own sqrt, and a String where a Number is declared
 *   is refused at the position C finds. The three loops add a field twenty
 *   times whichever way it is reached, sixty by C's multiplication.
 * Guarantees: all seven claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include <math.h>

/* (name n sum): n == 0 answers sum, else adds x and loops, x read by `read`. */
static mt_atom *loop(const char *name, mt_atom *read)
{
    return E("=", E(name, V("n"), V("sum")),
             E("if", E("==", V("n"), 0), V("sum"), E("let", V("x"), read, E(name, E("-", V("n"), 1), E("+", V("sum"), V("x"))))));
}

int main(void)
{
    metta *m = open_engine();
    mt_atom *arrow = E("->", "Number", "Number", "SortedPoint");
    require("(: SortedPoint (-> Number Number SortedPoint))", mt_add(m, E(":", "SortedPoint", mt_keep(arrow))));
    require("sorted-x", mt_add(m, E("=", E("sorted-x", E("SortedPoint", V("x"), V("y"))), V("x"))));
    require("plain-x", mt_add(m, E("=", E("plain-x", E("PlainPoint", V("x"), V("y"))), V("x"))));
    require("make-sorted-point", mt_add(m, E("=", E("make-sorted-point", V("x"), V("y")), E("SortedPoint", V("x"), V("y")))));
    require("sorted-norm", mt_add(m, E("=", E("sorted-norm", E("SortedPoint", V("x"), V("y"))),
                                       E("sqrt-math", E("+", E("*", V("x"), V("x")), E("*", V("y"), V("y")))))));
    const int64_t x = 3, y = 4;
    check_answers("a point's type is its arrow's last", mt_eval(m, E("get-type", E("SortedPoint", x, y))), mt_keep(mt_at(arrow, mt_len(arrow) - 1)));
    check_answers("a field", mt_eval(m, E("sorted-x", E("SortedPoint", x, y))), N(x));
    check_answers("a method over the fields", mt_eval(m, E("sorted-norm", E("SortedPoint", x, y))), mt_real(sqrt((double)(x * x + y * y))));
    mt_atom *bad = E("SortedPoint", T("bad"), y);
    check_answers("a text where a Number goes", mt_eval(m, E("make-sorted-point", T("bad"), y)),
                  E("Error", mt_keep(bad), E("BadArgType", 1, mt_keep(mt_at(arrow, 1)), "String")));
    mt_drop(bad);

    require("constructor-control", mt_add(m, loop("constructor-control", N(3))));
    require("constructor-sorted", mt_add(m, loop("constructor-sorted", E("sorted-x", E("SortedPoint", x, y)))));
    require("constructor-plain", mt_add(m, loop("constructor-plain", E("plain-x", E("PlainPoint", x, y)))));
    const char *loops[] = { "constructor-control", "constructor-sorted", "constructor-plain" };
    for (size_t i = 0; i < 3; i++) check_answers(loops[i], mt_eval(m, E(loops[i], 20, 0)), N(20 * x));
    mt_drop(arrow);
    return done(m);
}
