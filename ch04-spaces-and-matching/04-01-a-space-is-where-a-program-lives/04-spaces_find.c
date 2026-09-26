/* Purpose: a match used as a condition. lib_spaces' find answers True with
 *   the pattern's variables bound, once per match, or False when nothing
 *   matches, so a chain of friendships is a nested condition. The facts are
 *   a C table, and the same chains walked in C from mt_match() agree.
 * Guarantees: the chain answers (FoundChain a b c) then (MissedSecondPiece)
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>

/* A door the program needs before it can go on: on refusal, say why and stop. */
#define require(what, ok) \
    ((ok) ? (void)0 : (fprintf(stderr, "%s: %s\n", (what), mt_errmsg()), exit(EXIT_FAILURE)))

/* Whether a list holds exactly the children of want, in order, each equal
   up to renaming variables; takes both, and shows them when they differ. */
static inline bool list_is(mt_list got, mt_atom *want)
{
    bool holds = mt_ok() && want && got.len == mt_len(want);
    for (size_t i = 0; holds && i < got.len; i++)
        holds = mt_alpha_eq(got.items[i], mt_at(want, i));
    if (!holds) {
        fprintf(stderr, "  got");
        for (size_t i = 0; i < got.len; i++) fprintf(stderr, " %s", mt_show(got.items[i]));
        fprintf(stderr, "\n  want %s\n", want ? mt_show(want) : "nothing");
    }
    mt_list_free(got);
    mt_drop(want);
    return holds;
}

/* Whether a query answers exactly the children of want; takes both. */
static inline bool answers_are(mt_answers *answers, mt_atom *want)
{
    return list_is(mt_all(answers), want);
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
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
    assert(answers_are(mt_eval(m, chain), E(E("FoundChain", "a", "b", "c"), E("MissedSecondPiece")))
           && "find binds as a condition");

    /* The same walk in C: each first friendship, then its continuation. */
    size_t found = 0, missed = 0;
    mt_rows (first, mt_match(m, E("friend", V("a"), V("b")))) {
        size_t onward = 0;
        mt_each (next, mt_match(m, E("friend", mt_keep(mt_bound(first, "b")), V("c")))) onward++;
        if (onward) found += onward;
        else missed++;
    }
    assert(found == 1 && missed == 1 && "C's own walk finds one chain and one dead end");
    mt_close(m);
    return 0;
}
