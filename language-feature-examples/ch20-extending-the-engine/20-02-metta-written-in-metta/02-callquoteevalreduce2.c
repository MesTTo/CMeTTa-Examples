/* Purpose: four ways to hand one call over. fib is chapter 7's, installed
 *   from fib.h, and myfunc answers C's constant; each of call, quote, eval
 *   and reduce wraps (fib (myfunc)) in a definition C builds from its row
 *   of control_forms.h. The claims wrap each answer in a symbol of its own,
 *   so what is compared is what came back: C's fib of its constant where
 *   the form reduces, and the call as written, (myfunc) included, where
 *   quote hands it over.
 * Guarantees: all four claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include "../../ch07-control-flow/07-05-recursion/fib.h"
#include "control_forms.h"

enum { MYFUNC = 5 };

static mt_atom *inner(void) { return T_FIB(E("myfunc")); }

int main(void)
{
    metta *m = open_engine();
    require("fib", install_fib(m));
    require("myfunc", mt_add(m, E("=", E("myfunc"), MYFUNC)));
    for (size_t i = 0; i < CONTROL_FORMS; i++) {
        char definition[32];
        snprintf(definition, sizeof definition, "%s-fib", control_forms[i].name);
        require(definition, mt_add(m, E("=", E(definition), E(control_forms[i].name, inner()))));
    }
    for (size_t i = 0; i < CONTROL_FORMS; i++) {
        char definition[32], label[32];
        snprintf(definition, sizeof definition, "%s-fib", control_forms[i].name);
        snprintf(label, sizeof label, "fib-%s", control_forms[i].name);
        check_answers(label, mt_eval(m, E(label, E(definition))),
                      E(label, answered(&control_forms[i], inner(), N(fib(MYFUNC)))));
    }
    return done(m);
}
