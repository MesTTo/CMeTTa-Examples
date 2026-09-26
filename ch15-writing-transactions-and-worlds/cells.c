/* Purpose: a mutable cell is an atom C holds. new-state answers the cell,
 *   and the same atom, kept with mt_keep(), is passed to get-state and
 *   change-state! for every read and write.
 * Guarantees: the cell reads 0, takes 7 with True, and reads 7
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
    mt_atom *cell = mt_one(mt_eval(m, E("new-state", 0)));
    require("create a cell", cell != NULL);
    assert(mt_one_int(mt_eval(m, E("get-state", mt_keep(cell)))) == 0 && "it starts at 0");
    assert(answers_are(mt_eval(m, E("change-state!", mt_keep(cell), 7)), E(B(true)))
           && "change-state! answers True");
    assert(mt_one_int(mt_eval(m, E("get-state", mt_keep(cell)))) == 7 && "and it reads 7");
    mt_drop(cell);
    mt_close(m);
    return 0;
}
