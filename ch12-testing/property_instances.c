/* Purpose: a property checked by exhaustion. Over every pair of integers in
 *   -8..8, (pair $x $x) unifies with (pair a b) exactly when a equals b, the
 *   substitution rebuilds the instance, and equal atoms hash alike. No engine
 *   is needed for any of it.
 * Guarantees: all 289 pairs satisfy the three properties
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>

int main(void)
{
    mt_atom *diagonal = E("pair", V("x"), V("x"));
    bool unifies_on_diagonal = true, rebuilds = true, hashes_alike = true;
    for (int a = -8; a <= 8; a++)
        for (int b = -8; b <= 8; b++) {
            mt_atom *pair = E("pair", a, b);
            mt_bindings *bindings = mt_unify(diagonal, pair);
            unifies_on_diagonal &= (bindings != NULL) == (a == b);
            if (bindings) {
                mt_atom *instance = mt_substitute(diagonal, bindings);
                rebuilds &= mt_eq(instance, pair);
                hashes_alike &= mt_hash(instance) == mt_hash(pair);
                mt_drop(instance);
                mt_bindings_free(bindings);
            }
            mt_drop(pair);
        }
    assert(unifies_on_diagonal && "(pair $x $x) unifies exactly on the diagonal");
    assert(rebuilds && "the substitution rebuilds the instance");
    assert(hashes_alike && "equal atoms hash alike");
    mt_drop(diagonal);
    return 0;
}
