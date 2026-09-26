/* Purpose: a shipped library, imported. lib_roman's map-flat maps (+ 1)
 *   over a list, which C does over the same array with its own function.
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

static int64_t inc(int64_t x) { return x + 1; }
static const int64_t XS[] = { 1, 2, 3 };

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_roman", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_roman")))));
    mt_atom *mapped[3];
    for (size_t i = 0; i < 3; i++) mapped[i] = N(inc(XS[i]));
    assert(answers_are(mt_eval(m, E("map-flat", E("+", 1), E(1, 2, 3))), E(mt_exprv(3, mapped))) && "(map-flat (+ 1) (1 2 3))");
    mt_close(m);
    return 0;
}
