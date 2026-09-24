/* Purpose: a match used as a condition. lib_spaces' find answers True with
 *   the pattern's variables bound, once per match, or False when nothing
 *   matches, so a chain of friendships is a nested condition. The facts are
 *   a C table, and the same chains walked in C from mt_match() agree.
 * Guarantees: the chain answers (FoundChain a b c) then (MissedSecondPiece)
 *   [tested: make twins; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    require("import lib_spaces",
            mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_spaces")))));
    static const char *const friends[][2] = { {"a", "b"}, {"b", "c"} };
    for (size_t i = 0; i < 2; i++) require("store a friendship", mt_add(m, E("friend", friends[i][0], friends[i][1])));

    /* (if (find &self (friend $a $b))
           (if (find &self (friend $b $c)) (FoundChain $a $b $c) (MissedSecondPiece))
           (MissedAllPieces)) */
    mt_atom *chain = E("if", E("find", "&self", E("friend", V("a"), V("b"))),
                       E("if", E("find", "&self", E("friend", V("b"), V("c"))),
                         E("FoundChain", V("a"), V("b"), V("c")), E("MissedSecondPiece")),
                       E("MissedAllPieces"));
    check_answers("find binds as a condition", mt_eval(m, chain),
                  E("FoundChain", "a", "b", "c"), E("MissedSecondPiece"));

    /* The same walk in C: each first friendship, then its continuation. */
    size_t found = 0, missed = 0;
    mt_rows (first, mt_match(m, E("friend", V("a"), V("b")))) {
        size_t onward = 0;
        mt_each (next, mt_match(m, E("friend", mt_keep(mt_bound(first, "b")), V("c")))) onward++;
        if (onward) found += onward;
        else missed++;
    }
    check("C's own walk finds one chain and one dead end", found == 1 && missed == 1);
    return done(m);
}
