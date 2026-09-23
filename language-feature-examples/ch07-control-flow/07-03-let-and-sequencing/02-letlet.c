/* Purpose: a destructuring binding. f's let* unifies ($f1 $c1 3) with
 *   (1 2 $d1), binding a variable on each side. C does the same in pure C:
 *   mt_unify the pattern with the value and mt_substitute the result into
 *   the body, and the engine's (f) must answer what C computed.
 * Guarantees: the original's claim holds [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    require("f", mt_lower(m, (f), (let* ((($f1 $c1 3) (1 2 $d1))) ($f1 $c1 $d1))));

    mt_atom *pattern = E(V("f1"), V("c1"), 3), *value = E(1, 2, V("d1")), *body = E(V("f1"), V("c1"), V("d1"));
    mt_bindings *theta = mt_unify(pattern, value);
    require("the pattern unifies with the value", theta != NULL);
    mt_atom *computed = mt_substitute(body, theta);
    mt_bindings_free(theta);
    mt_drop(pattern);
    mt_drop(value);
    mt_drop(body);
    check_answers("(f) is the substituted body", mt_eval(m, E("f")), computed);
    return done(m);
}
