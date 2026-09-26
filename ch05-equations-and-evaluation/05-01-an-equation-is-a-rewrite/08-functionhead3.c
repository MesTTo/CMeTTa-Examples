/* Purpose: one constraint per argument. in holds of a member of a list,
 *   and myplus constrains its two arguments and its result with one let
 *   each, so it answers forward, refuses what falls out of range, and
 *   enumerates what reaches a value when its arguments are variables.
 * Guarantees: all six claims of the original hold
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
    /* (= (in $x $L) (let True (is-member $x $L) $x)) */
    require("define in", mt_add(m, E("=", E("in", V("x"), V("L")),
                                     E("let", B(true), E("is-member", V("x"), V("L")), V("x")))));
    /* (= (myplus $A $B) (let $A (in $X (1 2 3)) (let $B (in $Y (2 3)) (in (+ $X $Y) (3 4 5))))) */
    require("define myplus", mt_add(m, E("=", E("myplus", V("A"), V("B")),
        E("let", V("A"), E("in", V("X"), E(1, 2, 3)),
          E("let", V("B"), E("in", V("Y"), E(2, 3)),
            E("in", E("+", V("X"), V("Y")), E(3, 4, 5)))))));

    assert(answers_are(mt_eval(m, E("myplus", 1, 3)), E(4)) && "1 + 3 is in range");
    assert(!mt_first(mt_eval(m, E("myplus", 3, 3))) && mt_ok() && "3 + 3 is out of range");
    assert(!mt_first(mt_eval(m, E("myplus", 3, 4))) && mt_ok() && "4 is not an argument it takes");
    assert(answers_are(mt_eval(m, E("myplus", V("x"), 3)), E(4, 5)) && "adding anything to 3 reaches 4 and 5");
    assert(answers_are(mt_eval(m, E("myplus", V("x"), V("y"))), E(3, 4, 4, 5, 5)) && "adding anything to anything");
    assert(answers_are(mt_eval(m, E("let", B(true), E(">", E("myplus", V("x"), 2), 3), V("x"))), E(2, 3))
           && "which x added to 2 goes above 3");
    mt_close(m);
    return 0;
}
