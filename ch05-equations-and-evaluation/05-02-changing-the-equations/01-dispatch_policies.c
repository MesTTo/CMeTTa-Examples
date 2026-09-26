/* Purpose: what a call answers when no equation matches is a policy, and
 *   the policy is a fact in the catalog space. By default only-a answers
 *   nothing for B; a (dispatch-policy only-a NoMatchEnum NoMatchOriginal)
 *   row C adds to &metta makes the call answer itself, and removing the row
 *   restores the default.
 * Guarantees: nothing, then the call itself, then nothing again
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
    require("(= (only-a A) hit)", mt_add(m, E("=", E("only-a", "A"), "hit")));
    assert(!mt_first(mt_eval(m, E("only-a", "B"))) && mt_ok() && "by default a call no equation matches answers nothing");

    mt_space *catalog = mt_catalog(m);
    mt_atom *keep_the_call = E("dispatch-policy", "only-a", "NoMatchEnum", "NoMatchOriginal");
    require("override the policy for only-a", mt_add(catalog, mt_keep(keep_the_call)));
    assert(answers_are(mt_eval(m, E("only-a", "B")), E(E("only-a", "B"))) && "now the call answers itself");
    require("remove the override", mt_del(catalog, keep_the_call));
    assert(!mt_first(mt_eval(m, E("only-a", "B"))) && mt_ok() && "and the default is back");
    mt_close(m);
    return 0;
}
