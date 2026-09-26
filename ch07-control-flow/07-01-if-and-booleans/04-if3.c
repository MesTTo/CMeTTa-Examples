/* Purpose: an unbound variable is one. is-var asks the engine what
 *   mt_kind_of asks in C, and the nested if answers the arm C's ?: picks.
 * Guarantees: the original's claim holds [tested 2026-09-27T00:35:58+10:00:
 *   make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>

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
    mt_atom *a = V("A");
    assert(answers_are(mt_eval(m, E("if", E("is-var", mt_keep(a)), E("if", B(true), 42, "lol"), E("+", 2, 2))), E(mt_kind_of(a) == MT_VARIABLE ? (true ? N(42) : S("lol")) : N(2 + 2)))
           && "(if (is-var $A) (if True 42 lol) (+ 2 2))");
    mt_drop(a);
    mt_close(m);
    return 0;
}
