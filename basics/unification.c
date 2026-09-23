/* Purpose: Apply symmetric bindings and preserve repeated-variable constraints.
 * Owns resources: local C handles are released before exit; a failed check
 *   terminates the example process.
 * Guarantees: the assertions below hold [tested: make check; commit=6022c3f48b6dc64752c6e49cfe9d985c7ac7a4e9].
 * Open Obligations: None.
 */
#include "common.h"
int main(void)
{
    metta *m = open_engine();
    mt_atom *pattern = mt_expr("Parent", mt_var("parent"), mt_var("child"));
    mt_atom *fact = mt_expr("Parent", "Tom", "Bob");
    mt_bindings *bindings = mt_unify(pattern, fact);
    check("unification succeeds", bindings != NULL);
    mt_atom *template = mt_expr("Cares", mt_var("parent"), mt_var("child"));
    mt_atom *answer = mt_substitute(template, bindings);
    check_atom("substituted template", answer, "(Cares Tom Bob)");
    mt_drop(answer); mt_drop(template); mt_bindings_free(bindings);
    mt_drop(pattern); mt_drop(fact);
    pattern = mt_expr("pair", mt_var("x"), mt_var("x"));
    fact = mt_expr("pair", 1, 2);
    bindings = mt_unify(pattern, fact);
    check("repeated variable refuses unequal fields", bindings == NULL && mt_ok());
    mt_drop(pattern); mt_drop(fact);
    return done(m, "unification");
}
