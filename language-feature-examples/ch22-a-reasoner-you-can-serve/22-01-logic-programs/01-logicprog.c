/* Purpose: a relation searched recursively. successor is C's table of
 *   adjacent letters, one equation per row answering True, and
 *   later-in-alphabet's two equations, one step and one step followed by the
 *   relation again, are built as the terms they are. Asked from d, the
 *   search answers each letter below it, nearest first, which is the order
 *   C's own walk down its alphabet finds them. Both relations fail at their
 *   boundary rather than answer themselves, the dispatch policy written into
 *   the catalog from vocabularies.h's NoMatchEnum words.
 * Guarantees: the original's claim holds [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

static const char *const alphabet[] = { "a", "b", "c", "d", "e", "f", "g" };
#define LETTERS (sizeof alphabet / sizeof *alphabet)

int main(void)
{
    metta *m = open_engine();
    static const char *const relations[] = { "successor", "later-in-alphabet" };
    for (size_t i = 0; i < 2; i++)
        require("fail at the boundary", mt_add(mt_catalog(m), E("dispatch-policy", relations[i], "NoMatchEnum",
                                                               mt_no_match_enum_names[MT_NO_MATCH_ENUM_NO_MATCH_FAIL])));
    for (size_t i = 1; i < LETTERS; i++)
        require("a successor row", mt_add(m, E("=", E("successor", alphabet[i], alphabet[i - 1]), B(true))));
    require("one step", mt_add(m, E("=", E("later-in-alphabet", V("X"), V("Y")), E("successor", V("X"), V("Y")))));
    require("and further",
            mt_add(m, E("=", E("later-in-alphabet", V("X"), V("Y")),
                        E("and", E("successor", V("X"), V("Z")), E("later-in-alphabet", V("Z"), V("Y"))))));

    const size_t from = 3;
    mt_atom *want[LETTERS];
    size_t n = 0;
    for (size_t i = from; i-- > 0;) want[n++] = E(B(true), alphabet[i]);
    check_answers_("every letter below d, nearest first",
                   mt_eval(m, E(E("later-in-alphabet", alphabet[from], V("1")), V("1"))), n, want);
    return done(m);
}
