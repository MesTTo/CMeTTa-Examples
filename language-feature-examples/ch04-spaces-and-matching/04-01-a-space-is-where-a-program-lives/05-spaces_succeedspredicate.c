/* Purpose: a predicate that binds. lib_spaces' succeedsPredicate asks a
 *   space whether (friend x y) holds, spelled (&self friend x y): a ground
 *   question answers False when nothing holds, and a question with
 *   variables binds them when something does.
 * Guarantees: (friend tim tom) is False, and after (friend a b) is stored
 *   the binding question answers (a b) [tested: make twins; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    require("import lib_spaces",
            mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_spaces")))));
    check("nothing holds, so the ground question is False",
          !mt_one_truth(mt_eval(m, E("succeedsPredicate", E("&self", "friend", "tim", "tom")))) && mt_ok());

    require("store (friend a b)", mt_add(m, E("friend", "a", "b")));
    /* (if (succeedsPredicate (&self friend $a $b)) ($a $b) NotFound) */
    check_answers("the binding question answers what it bound",
                  mt_eval(m, E("if", E("succeedsPredicate", E("&self", "friend", V("a"), V("b"))),
                               E(V("a"), V("b")), "NotFound")),
                  E("a", "b"));
    return done(m);
}
