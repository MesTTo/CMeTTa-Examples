/* Purpose: an import order that does not matter. index imports uses before
 *   defines, so the caller uses writes names a callee defines has not
 *   written yet; the index's own test runs as it imports, and afterwards the
 *   caller answers what defines says its callee answers.
 * Guarantees: the original states no claim of its own, and the one its
 *   imported index states holds from C as well [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include "modules.h"

int main(void)
{
    metta *m = open_engine();
    import_fixture(m, "&self", "imports/import_order/index");
    check_answers("the caller reaches the callee imported after it", mt_eval(m, E("import-order-caller")), "import-order-ok");
    return done(m);
}
