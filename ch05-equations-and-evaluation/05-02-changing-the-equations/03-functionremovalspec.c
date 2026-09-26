/* Purpose: removing an equation from a specialized function. (f g) makes the
 *   engine specialize f on g; taking one of f's equations out afterwards
 *   still leaves the specialized call answering from the one that remains,
 *   and putting it back adds its answer after the other's.
 * Guarantees: (f g) answers 2 and 3, then 3, then 3 and 2
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
    require("(= (g $x) (+ $x 1))", mt_add(m, E("=", E("g", V("x")), E("+", V("x"), 1))));
    mt_atom *first = E("=", E("f", V("g")), E(V("g"), 1));      /* (= (f $g) ($g 1)) */
    require("(= (f $g) ($g 1))", mt_add(m, mt_keep(first)));
    require("(= (f $g) ($g 2))", mt_add(m, E("=", E("f", V("g")), E(V("g"), 2))));
    assert(answers_are(mt_eval(m, E("f", "g")), E(2, 3)) && "both equations answer");

    require("take the first out", mt_del(m, mt_keep(first)));
    assert(answers_are(mt_eval(m, E("f", "g")), E(3)) && "the specialized call answers from the one left");
    require("put it back", mt_add(m, first));
    assert(answers_are(mt_eval(m, E("f", "g")), E(3, 2)) && "and it answers after the other now");
    mt_close(m);
    return 0;
}
