/* Purpose: one definition kind inside another. A define inside a define is
 *   an ordinary call, which C computes as its own twice of twice; a Python
 *   operation inside a define is one crossing, held against C's toupper; and
 *   a body can write an equation, which C builds as the atom the body adds,
 *   so the new name answers that atom's body and a match finds it.
 * Guarantees: all four claims of the original hold, with its one unasserted
 *   form checked as well [tested: make twins; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"
#include <ctype.h>

static int64_t twice(int64_t x) { return x + x; }

int main(void)
{
    metta *m = open_engine();
    require("dc-twice", mt_add(m, E("=", E("dc-twice", V("x")), E("+", V("x"), V("x")))));
    require("dc-quad", mt_add(m, E("=", E("dc-quad", V("x")), E("dc-twice", E("dc-twice", V("x"))))));
    check_answers("a define inside a define", mt_eval(m, E("dc-quad", 5)), N(twice(twice(5))));
    require("dc-upper", mt_add(m, E("=", E("dc-upper", V("s")), E("py-call", E(".upper", V("s"))))));
    const char word[] = "ab";
    char upper[sizeof word];
    for (size_t i = 0; i < sizeof word; i++) upper[i] = (char)toupper((unsigned char)word[i]);
    check_answers("a Python operation inside a define", mt_eval(m, E("dc-upper", T(word))), S(upper));
    mt_atom *nine = E("=", E("dc-nine"), 9);
    require("dc-install", mt_add(m, E("=", E("dc-install"), E("add-atom", "&self", mt_keep(nine)))));
    check_answers("the body adds an equation", mt_eval(m, E("dc-install")), B(true));
    check_answers("and the new name answers", mt_eval(m, E("dc-nine")), mt_keep(mt_at(nine, 2)));
    check_answers("an equation is an atom to match", mt_eval(m, E("match", "&self", E("=", E("dc-nine"), V("body")), V("body"))), mt_keep(mt_at(nine, 2)));
    mt_drop(nine);
    return done(m);
}
