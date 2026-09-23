/* Purpose: a shipped library, imported. lib_roman's map-flat maps (+ 1)
 *   over a list, which C does over the same array with its own function.
 * Guarantees: the original's claim holds [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

static int64_t inc(int64_t x) { return x + 1; }
static const int64_t XS[] = { 1, 2, 3 };

int main(void)
{
    metta *m = open_engine();
    require("import lib_roman", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_roman")))));
    mt_atom *mapped[3];
    for (size_t i = 0; i < 3; i++) mapped[i] = N(inc(XS[i]));
    check_answers("(map-flat (+ 1) (1 2 3))", mt_eval(m, E("map-flat", E("+", 1), E(1, 2, 3))), mt_exprv(3, mapped));
    return done(m);
}
