/* Purpose: what a call answers when no equation matches is a policy, and
 *   the policy is a fact in the catalog space. By default only-a answers
 *   nothing for B; a (dispatch-policy only-a NoMatchEnum NoMatchOriginal)
 *   row C adds to &metta makes the call answer itself, and removing the row
 *   restores the default.
 * Guarantees: nothing, then the call itself, then nothing again [tested:
 *   make twins; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    require("(= (only-a A) hit)", mt_add(m, E("=", E("only-a", "A"), "hit")));
    check_none("by default a call no equation matches answers nothing", mt_eval(m, E("only-a", "B")));

    mt_space *catalog = mt_catalog(m);
    mt_atom *keep_the_call = E("dispatch-policy", "only-a", "NoMatchEnum", "NoMatchOriginal");
    require("override the policy for only-a", mt_add(catalog, mt_keep(keep_the_call)));
    check_answers("now the call answers itself", mt_eval(m, E("only-a", "B")), E("only-a", "B"));
    require("remove the override", mt_del(catalog, keep_the_call));
    check_none("and the default is back", mt_eval(m, E("only-a", "B")));
    return done(m);
}
