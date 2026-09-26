/* Purpose: solving for a boolean. and and or are relational, so the engine
 *   answers every pair of values satisfying (and (or $x True) $y); C walks
 *   the same two-valued space with two loops, True before False as the
 *   engine tries them, and keeps the pairs its own && and || accept.
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

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    mt_atom *satisfying[4];
    size_t n = 0;
    for (int x = 1; x >= 0; x--)
        for (int y = 1; y >= 0; y--)
            if ((x || true) && y) satisfying[n++] = E(B(x), B(y));
    assert(list_is(mt_all(mt_eval(m, E("if", E("and", E("or", V("x"), B(true)), V("y")), E(V("x"), V("y"))))), mt_exprv(n, satisfying))
           && "the pairs that satisfy the condition");
    mt_close(m);
    return 0;
}
