/* Purpose: one file imported twice, under two spellings, loads once, and a
 *   cycle of two files loads both. The duplicate's marker answers exactly
 *   once, and each half of the cycle answers its own symbol.
 * Guarantees: all three claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include "_fixtures/modules.h"

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
    static const char *const imported[] = {
        "imports/overhaul/duplicate", "imports/overhaul/./duplicate.metta", "imports/overhaul/cycle_a",
    };
    for (size_t i = 0; i < sizeof imported / sizeof *imported; i++) import_fixture(m, "&self", imported[i]);
    assert(answers_are(mt_eval(m, E("duplicate-import-result")), E("loaded-once")) && "the duplicate loaded once");
    static const char *const halves[][2] = { { "cycle-a", "a" }, { "cycle-b", "b" } };
    for (size_t i = 0; i < 2; i++) assert(answers_are(mt_eval(m, E(halves[i][0])), E(halves[i][1])) && "each half of the cycle loaded");
    mt_close(m);
    return 0;
}
