/* Purpose: lets and superpositions stacked four ways, each equation built as
 *   an atom, then collapsed together. program1 collapses 12 and its argument plus 4,
 *   program2 fans out a collapsed list, program3 branches into (42 43), and
 *   program4 gathers the three calls: the three fan-outs of program2 each
 *   carry the other two answers. C builds the expected rows in a loop.
 * Guarantees: the original's claim holds [tested 2026-09-27T00:35:58+10:00:
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
    require("program1", mt_add(m, E("=", E("program1", V("Y")),
                                   E("let", V("X"), V("Y"), E("collapse", E("superpose", E(12, E("+", V("X"), 4))))))));
    require("program2", mt_add(m, E("=", E("program2", V("Y")),
                                   E("let", V("list"), E("let", V("L"), E(1, 2, 3), E("collapse", E("superpose", V("L")))),
                                     E("superpose", V("list"))))));
    require("program3", mt_add(m, E("=", E("program3", V("x")),
                                   E("if", E("==", V("x"), 2),
                                     E("let", V("z"), E("superpose", E(E("if", E("<", V("x"), 10), E("superpose", E(E(42, 43))), 43))), V("z")),
                                     E("let", V("z"), 4, V("z"))))));
    require("program4", mt_add(m, E("=", E("program4"), E("collapse", E(E("program1", 42), E("program2", 42), E("program3", 2))))));

    mt_atom *rows[3];
    for (int64_t n = 1; n <= 3; n++) rows[n - 1] = E(E(12, 42 + 4), n, E(42, 43));
    assert(answers_are(mt_eval(m, E("program4")), E(mt_exprv(3, rows))) && "(program4)");
    mt_close(m);
    return 0;
}
