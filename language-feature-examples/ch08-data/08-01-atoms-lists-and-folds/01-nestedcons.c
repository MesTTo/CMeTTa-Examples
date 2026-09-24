/* Purpose: two cons cells in one head pattern. f's head destructures its
 *   argument twice and answers the second element; C holds the same list as
 *   an array of children, where the second element is mt_at(list, 1).
 * Guarantees: the original's claim holds [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    require("(= (f (cons $a (cons $b $L))) $b)",
            mt_add(m, E("=", E("f", E("cons", V("a"), E("cons", V("b"), V("L")))), V("b"))));
    mt_atom *list = E("a", "b", "c", "d");
    check_answers("the second element", mt_eval(m, E("f", mt_keep(list))), mt_keep(mt_at(list, 1)));
    mt_drop(list);
    return done(m);
}
