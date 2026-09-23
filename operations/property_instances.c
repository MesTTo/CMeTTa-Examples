/* Purpose: Exhaust finite record instances against repeated-variable unification.
 * Owns resources: local C handles are released before exit; a failed check
 *   terminates the example process.
 * Guarantees: the assertions below hold [tested: make check; commit=6022c3f48b6dc64752c6e49cfe9d985c7ac7a4e9].
 * Open Obligations: None.
 */
#include "common.h"
int main(void)
{
    mt_atom *pattern = mt_expr("pair", mt_var("x"), mt_var("x"));
    for (int a = -8; a <= 8; ++a) {
        for (int b = -8; b <= 8; ++b) {
            mt_atom *fact = mt_expr("pair", a, b);
            mt_bindings *bindings = mt_unify(pattern, fact);
            check("unifies exactly on the diagonal", (bindings != NULL) == (a == b));
            if (bindings) {
                mt_atom *instance = mt_substitute(pattern, bindings);
                check("substitution reconstructs the instance", mt_eq(instance, fact));
                check("equal values hash alike", mt_hash(instance) == mt_hash(fact));
                mt_drop(instance); mt_bindings_free(bindings);
            }
            mt_drop(fact);
        }
    }
    mt_drop(pattern);
    return done(NULL, "property_instances");
}
