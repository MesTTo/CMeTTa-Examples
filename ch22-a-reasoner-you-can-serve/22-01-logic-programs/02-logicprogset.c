/* Purpose: a set found by constraining it. myf holds for a list that has a
 *   and b as members and has exactly two items; asked with the list unknown,
 *   the engine builds the list it describes, and C expects the list of its
 *   required members in the order the conjunction asks for them, whose size
 *   is their count.
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

static const char *const members[] = { "a", "b" };
#define MEMBERS (sizeof members / sizeof *members)

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("myf", mt_add(m, E("=", E("myf", V("M")),
                                E("and", E("and", E("member", members[0], V("M")), E("member", members[1], V("M"))),
                                  E("==", E("size-atom", V("M")), (int64_t)MEMBERS)))));
    mt_atom *want[MEMBERS];
    for (size_t i = 0; i < MEMBERS; i++) want[i] = S(members[i]);
    assert(answers_are(mt_eval(m, E("if", E("once", E("myf", V("M"))), V("M"))), E(mt_exprv(MEMBERS, want)))
           && "the list the constraints describe");
    mt_close(m);
    return 0;
}
