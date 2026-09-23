/* Purpose: a query prepared once reads current facts. The pattern is an atom
 *   the program keeps, handed to each mt_query() with mt_keep(), and every
 *   call opens a fresh engine query, so a fact added between two calls is
 *   seen by the second.
 * Guarantees: the same pattern finds Ada, then Bob after Bob is added
 *   [tested: make check; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    mt_atom *score = E("score", V("who"), V("n"));
    require("Ada scores 7", mt_add(m, E("score", "Ada", 7)));
    check_answers("scores above 5", mt_query(m, mt_keep(score), E(">", V("n"), 5)),
                  E("score", "Ada", 7));
    require("Bob scores 9", mt_add(m, E("score", "Bob", 9)));
    check_answers("the same pattern sees the new fact",
                  mt_query(m, mt_keep(score), E(">", V("n"), 8)), E("score", "Bob", 9));
    mt_drop(score);
    return done(m);
}
