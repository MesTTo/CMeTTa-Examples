/* Purpose: one file imported twice, under two spellings, loads once, and a
 *   cycle of two files loads both. The duplicate's marker answers exactly
 *   once, and each half of the cycle answers its own symbol.
 * Guarantees: all three claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"
#include "modules.h"

int main(void)
{
    metta *m = open_engine();
    static const char *const imported[] = {
        "imports/overhaul/duplicate", "imports/overhaul/./duplicate.metta", "imports/overhaul/cycle_a",
    };
    for (size_t i = 0; i < sizeof imported / sizeof *imported; i++) import_fixture(m, "&self", imported[i]);
    check_answers("the duplicate loaded once", mt_eval(m, E("duplicate-import-result")), "loaded-once");
    static const char *const halves[][2] = { { "cycle-a", "a" }, { "cycle-b", "b" } };
    for (size_t i = 0; i < 2; i++) check_answers("each half of the cycle loaded", mt_eval(m, E(halves[i][0])), halves[i][1]);
    return done(m);
}
