/* Purpose: sequencing. A run of C statements is progn: write a fact, take it
 *   back, write another, and read what is left. progn keeps its last value
 *   as C's comma operator does, and prog1 keeps its first.
 * Guarantees: all three claims of the original hold
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
    require("write (friend sam tom)", mt_add(m, E("friend", "sam", "tom")));
    require("take it back", mt_del(m, E("friend", "sam", "tom")));
    require("write (friend sam tim)", mt_add(m, E("friend", "sam", "tim")));
    assert(answers_are(mt_eval(m, E("match", "&self", E("friend", "sam", V("who")), V("who"))), E("tim")) && "what is left");

    assert(answers_are(mt_eval(m, E("prog1", 1, 2, 3)), E(1)) && "prog1 keeps the first");
    assert(answers_are(mt_eval(m, E("progn", 1, 2, 3)), E(((void)1, (void)2, 3)))
           && "progn keeps the last, as the comma operator does");
    mt_close(m);
    return 0;
}
