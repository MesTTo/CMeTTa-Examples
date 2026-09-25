/* Purpose: bindings as a value. mylet hands let* its bindings as an
 *   argument, declared Atom so the body arrives unevaluated, and C builds
 *   the bindings it hands over. A binding is a (pattern value) pair and a
 *   pattern that does not match gives no answer; noeval hands bindings to a
 *   definition that evaluates its arguments; bindings that are not pairs are
 *   refused, which C sees as MT_ERROR on the cursor; and (let* foo ok) is no
 *   binding at all but a partial application, which arrives as the
 *   expression (partial let* (foo ok)) and compares as the term C builds.
 * Guarantees: all eight claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

/* (( $x 1 ) ( $y 2 )) */
static mt_atom *x_and_y(void) { return E(E(V("x"), 1), E(V("y"), 2)); }

int main(void)
{
    metta *m = open_engine();
    require("(: mylet (-> Atom Atom %Undefined%))",
            mt_add(m, E(":", "mylet", E("->", "Atom", "Atom", "%Undefined%"))));
    require("mylet", mt_add(m, E("=", E("mylet", V("bindings"), V("body")), E("let*", V("bindings"), V("body")))));
    require("mylet-evaluating",
            mt_add(m, E("=", E("mylet-evaluating", V("bindings"), V("body")), E("let*", V("bindings"), V("body")))));

    check_answers("handed over", mt_eval(m, E("mylet", x_and_y(), E("+", V("x"), V("y")))), 3);
    check_answers("written out", mt_eval(m, E("let*", x_and_y(), E("+", V("x"), V("y")))), 3);
    check_answers("a binding is a pattern", mt_eval(m, E("mylet", E(E(E(V("a"), V("b")), E(1, 2))), V("b"))), 2);
    check_answers("a pattern that matches", mt_eval(m, E("mylet", E(E(5, 5)), "matched")), "matched");
    check_none("and one that does not", mt_eval(m, E("mylet", E(E(5, 6)), "matched")));
    check_answers("noeval carries bindings as data",
                  mt_eval(m, E("mylet-evaluating", E("noeval", E(E(V("x"), 1))), V("x"))), 1);

    mt_clear();
    mt_list refused = mt_all(mt_eval(m, E("mylet-evaluating", E("noeval", E(E(1, 2, 3))), "done")));
    check("bindings that are not pairs are refused", refused.len == 0 && mt_error() == MT_ERROR);
    mt_list_free(refused);
    mt_clear();

    check_answers("no list is no bindings: a partial application", mt_eval(m, E("let*", "foo", "ok")),
                  E("partial", "let*", E("foo", "ok")));
    return done(m);
}
