/* Purpose: a metatype is what the engine makes of an atom, and C's mt_kind
 *   is what the atom is; metatype() in common.h maps the one onto the other.
 *   They agree for every atom here but one: + is a symbol as C builds it and
 *   Grounded as the engine reads it, because the engine resolves that name
 *   to a built-in operation.
 * Guarantees: all six claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    mt_atom *atoms[] = { E("foo", 1, 2), E("a", "b"), N(1), V("x"), S("a") };
    for (size_t i = 0; i < sizeof atoms / sizeof *atoms; i++) {
        char *label = mt_show_dup(atoms[i]);
        check_answers(label, mt_eval(m, E("get-metatype", mt_keep(atoms[i]))), S(metatype(atoms[i])));
        mt_free(label), mt_drop(atoms[i]);
    }
    mt_atom *plus = S("+");
    check("+ is a symbol as C builds it", mt_kind_of(plus) == MT_SYMBOL);
    check_answers("and Grounded as the engine reads it", mt_eval(m, E("get-metatype", plus)), S("Grounded"));
    return done(m);
}
