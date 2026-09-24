/* Purpose: reading a body, then running it. Matching f's equation returns
 *   its body, specialised to the call, as data; C holds that atom and runs
 *   it with mt_eval, which is what eval is. evalCustom emulates eval by
 *   storing the body as myfunc, reducing it and taking it back out, and C's
 *   own statements do the same three steps.
 * Guarantees: both claims of the original hold, and the C emulation agrees
 *   [tested: make twins; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    require("f", mt_lower(m, (f $L $a $b), (let $result (+ $a $b) (append ($result) $L))));
    require("evalCustom", mt_lower(m, (evalCustom $body),
                                   (let* (($a (add-atom &self (= (myfunc) $body)))
                                          ($res (reduce (myfunc)))
                                          ($r (remove-atom &self (= (myfunc) $body))))
                                         $res)));

    mt_atom *body = NULL;
    mt_rows (row, mt_match(m, E("=", E("f", E(42), 40.7, 2), V("x")))) {
        mt_drop(body);
        body = mt_keep(mt_bound(row, "x"));
    }
    require("the specialised body", body != NULL);
    check_answers("running the body is mt_eval", mt_eval(m, mt_keep(body)), E(42.7, 42));
    check_answers("evalCustom emulates it", mt_eval(m, E("evalCustom", mt_keep(body))), E(42.7, 42));

    require("store (= (myfunc) body)", mt_add(m, E("=", E("myfunc"), mt_keep(body))));
    check_answers("so do C's statements", mt_eval(m, E("myfunc")), E(42.7, 42));
    require("take it back out", mt_del(m, E("=", E("myfunc"), body)));
    return done(m);
}
