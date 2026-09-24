/* Purpose: a set found by constraining it. myf holds for a list that has a
 *   and b as members and has exactly two items; asked with the list unknown,
 *   the engine builds the list it describes, and C expects the list of its
 *   required members in the order the conjunction asks for them, whose size
 *   is their count.
 * Guarantees: the original's claim holds [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

static const char *const members[] = { "a", "b" };
#define MEMBERS (sizeof members / sizeof *members)

int main(void)
{
    metta *m = open_engine();
    require("myf", mt_add(m, E("=", E("myf", V("M")),
                                E("and", E("and", E("member", members[0], V("M")), E("member", members[1], V("M"))),
                                  E("==", E("size-atom", V("M")), (int64_t)MEMBERS)))));
    mt_atom *want[MEMBERS];
    for (size_t i = 0; i < MEMBERS; i++) want[i] = S(members[i]);
    check_answers("the list the constraints describe", mt_eval(m, E("if", E("once", E("myf", V("M"))), V("M"))),
                  mt_exprv(MEMBERS, want));
    return done(m);
}
