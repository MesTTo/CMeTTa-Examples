/* Purpose: unification in C, with no engine call. mt_unify() binds variables
 *   on either side, mt_substitute() applies the bindings to a template, and a
 *   repeated variable demands equal fields.
 * Guarantees: the template instantiates, and (pair $x $x) refuses (pair 1 2)
 *   [tested: make check; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    mt_atom *pattern = E("Parent", V("parent"), V("child"));
    mt_atom *fact = E("Parent", "Tom", "Bob");
    mt_bindings *bindings = mt_unify(pattern, fact);
    require("the fact unifies", bindings != NULL);
    mt_atom *template = E("Cares", V("parent"), V("child"));
    check_atom("the bindings fill the template", mt_substitute(template, bindings),
               E("Cares", "Tom", "Bob"));
    mt_drop(template);
    mt_bindings_free(bindings);
    mt_drop(pattern);
    mt_drop(fact);

    mt_atom *diagonal = E("pair", V("x"), V("x"));
    mt_atom *unequal = E("pair", 1, 2);
    mt_clear();
    check("a repeated variable refuses unequal fields, without an error",
          mt_unify(diagonal, unequal) == NULL && mt_ok());
    mt_drop(diagonal);
    mt_drop(unequal);

    /* The engine's own unify agrees. */
    check_answers("and so does the engine",
                  mt_eval(m, E("unify", E("pair", V("x"), V("x")), E("pair", 1, 2), "same", "different")),
                  "different");
    return done(m);
}
