/* Purpose: a table stays true to its equations. pick answers one, and tabled
 *   it answers one from its table; a second equation added beside the
 *   first makes the table stale, and the engine's change funnel drops it, so
 *   the next call answers one and two; removing the first equation, as an
 *   atom C builds, leaves two. A tabled call answers from SWI's answer trie
 *   rather than in clause order, so the pair is compared as a set: sorted
 *   with qsort and mt_order, which is sort-atom's order.
 * Guarantees: all four claims of the original hold
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

/* Every answer of a call, in the standard order. TAKES goal. */
static mt_list sorted(metta *m, mt_atom *goal)
{
    mt_list answers = mt_all(mt_eval(m, goal));
    qsort(answers.items, answers.len, sizeof *answers.items, mt_order);
    return answers;
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_tabling", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_tabling")))));
    require("(pick $x) is one", mt_add(m, E("=", E("pick", V("x")), S("one"))));
    require("table pick", mt_one_truth(mt_eval(m, E("tabled", E("pick", V("x"))))));
    for (int i = 0; i < 2; i++) assert(answers_are(mt_eval(m, E("pick", "a")), E(S("one"))) && "one, from the table");

    require("(pick $x) is two as well", mt_add(m, E("=", E("pick", V("x")), S("two"))));
    assert(list_is(sorted(m, E("pick", "a")), E("one", "two")) && "the change reaches the table");
    require("remove the first", mt_del(m, E("=", E("pick", V("x")), "one")));
    assert(answers_are(mt_eval(m, E("pick", "a")), E(S("two"))) && "and the removal");
    mt_close(m);
    return 0;
}
