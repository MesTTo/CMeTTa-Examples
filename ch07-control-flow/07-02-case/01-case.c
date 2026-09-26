/* Purpose: case takes the first branch that matches, and a variable pattern
 *   matches anything, so it is C's switch with a default. casetest is an
 *   equation built as an atom and the C function beside it is the switch;
 *   the engine's answer must be the switch's.
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

static int64_t casetest(int64_t x)
{
    switch (x) {
    case 4: return 42;
    default: return 44;          /* ($otherpattern 44); ($otherother $45) is never reached */
    }
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("casetest", mt_add(m, E("=", E("casetest", V("x")),
                                   E("case", V("x"), E(E(4, 42), E(V("otherpattern"), 44), E(V("otherother"), V("45")))))));
    assert(answers_are(mt_eval(m, E("casetest", 5)), E(casetest(5))) && "(casetest 5)");
    mt_close(m);
    return 0;
}
