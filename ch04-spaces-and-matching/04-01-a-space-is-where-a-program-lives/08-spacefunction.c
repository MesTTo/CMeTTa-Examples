/* Purpose: an equation is an atom, so removing the atom removes the
 *   definition. f and g are the same equation over different names; mt_del()
 *   takes f's out of &self and (f 3 4) is left with nothing to reduce it,
 *   while (g 3 4) still computes. A plain fact comes and goes the same way.
 * Guarantees: (f 3 4) answers itself, (g 3 4) is 7, and (my test) is gone
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

/* (= (name $x $y) (+ $x $y)) */
static mt_atom *sum_equation(const char *name)
{
    return E("=", E(name, V("x"), V("y")), E("+", V("x"), V("y")));
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("define f", mt_add(m, sum_equation("f")));
    require("define g", mt_add(m, sum_equation("g")));
    require("remove f's equation", mt_del(m, sum_equation("f")));

    assert(answers_are(mt_eval(m, E("f", 3, 4)), E(E("f", 3, 4))) && "(f 3 4) has nothing left to reduce it");
    assert(mt_one_int(mt_eval(m, E("g", 3, 4))) == 7 && "(g 3 4) is 7");

    require("store (my test)", mt_add(m, E("my", "test")));
    require("remove it", mt_del(m, E("my", "test")));
    assert(!mt_first(mt_match(m, E("my", "test"))) && mt_ok() && "(my test) is gone");
    mt_close(m);
    return 0;
}
