/* Purpose: a predicate that binds. lib_spaces' succeedsPredicate asks a
 *   space whether (friend x y) holds, spelled (&self friend x y): a ground
 *   question answers False when nothing holds, and a question with
 *   variables binds them when something does.
 * Guarantees: (friend tim tom) is False, and after (friend a b) is stored
 *   the binding question answers (a b) [tested 2026-09-27T00:35:58+10:00:
 *   make -C extensions/cmetta corpus-check].
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
    assert(!mt_one_truth(mt_eval(m, E("succeedsPredicate", E("&self", "friend", "tim", "tom")))) && mt_ok()
           && "nothing holds, so the ground question is False");

    require("store (friend a b)", mt_add(m, E("friend", "a", "b")));
    /* (if (succeedsPredicate (&self friend $a $b)) ($a $b) NotFound) */
    assert(answers_are(mt_eval(m, E("if", E("succeedsPredicate", E("&self", "friend", V("a"), V("b"))),
                                    E(V("a"), V("b")), "NotFound")), E(E("a", "b")))
           && "the binding question answers what it bound");
    mt_close(m);
    return 0;
}
