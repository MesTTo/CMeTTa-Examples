/* Purpose: a triangular walk under iterate. quad-step walks (t i sum) over
 *   the lower triangle of 1000 rows, adding t*i at each cell; C walks the
 *   same triangle with two nested loops, and the engine's sum must be C's.
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

static int64_t quad_sum(int64_t n)
{
    int64_t sum = 0;
    for (int64_t t = 1; t <= n; t++)
        for (int64_t i = 1; i <= t; i++) sum += t * i;
    return sum;
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_patrick", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_patrick")))));
    require("quad-step", mt_add(m, E("=", E("quad-step", V("dummy"), E(V("t"), V("i"), V("sum"))),
                                    E("if", E("==", V("i"), V("t")),
                                      E(E("+", V("t"), 1), 1, E("+", V("sum"), E("*", V("t"), V("i")))),
                                      E(V("t"), E("+", V("i"), 1), E("+", V("sum"), E("*", V("t"), V("i"))))))));
    require("quad-sum", mt_add(m, E("=", E("quad-sum", V("n")),
                                   E("last", E("iterate", 0, E("/", E("*", V("n"), E("+", V("n"), 1)), 2), E(1, 1, 0), "quad-step")))));
    assert(answers_are(mt_eval(m, E("quad-sum", 1000)), E(quad_sum(1000))) && "(quad-sum 1000)");
    mt_close(m);
    return 0;
}
