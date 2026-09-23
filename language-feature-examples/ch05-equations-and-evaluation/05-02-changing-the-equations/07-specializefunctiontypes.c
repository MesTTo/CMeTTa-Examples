/* Purpose: types survive specialization. f carries two arrow types; calling
 *   (f g 42) specializes it into f_Spec_[g], and both arrows are declared
 *   for the copy, which a match on the space finds.
 * Guarantees: (f g 42) answers (repra (g 42)) and both declarations of the
 *   specialized copy are in the space [tested: make twins; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    require("(= (g $x) $x)", mt_add(m, E("=", E("g", V("x")), V("x"))));
    require("(: f (-> Atom Number Atom))", mt_add(m, E(":", "f", E("->", "Atom", "Number", "Atom"))));
    require("(: f (-> Atom String Atom))", mt_add(m, E(":", "f", E("->", "Atom", "String", "Atom"))));
    require("(= (f $g $x) (repra ($g $x)))",
            mt_add(m, E("=", E("f", V("g"), V("x")), E("repra", E(V("g"), V("x"))))));

    check_answers("the call that specializes f", mt_eval(m, E("f", "g", 42)), E("repra", E("g", 42)));
    /* The copy's name is the engine's own, and C spells it as a symbol. */
    check_answers("its Number arrow is declared for the copy",
                  mt_match(m, E(":", "f_Spec_[g]", E("->", "Atom", "Number", "Atom"))),
                  E(":", "f_Spec_[g]", E("->", "Atom", "Number", "Atom")));
    check_answers("and its String arrow",
                  mt_match(m, E(":", "f_Spec_[g]", E("->", "Atom", "String", "Atom"))),
                  E(":", "f_Spec_[g]", E("->", "Atom", "String", "Atom")));
    return done(m);
}
