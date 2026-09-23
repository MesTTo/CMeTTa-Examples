/* Purpose: the same program built three ways against an installed cmetta:
 *   directly, through pkg-config and through CMake.
 * Owns resources: the runtime, closed after the answer is checked.
 * Guarantees: an installed library boots its installed engine and answers
 *   [tested: make check-consumers; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    check_int("the installed runtime answers", mt_one_int(mt_eval(m, E("+", 20, 22))), 42);
    check_text("the library is the header's version", mt_version(), MT_VERSION);
    return done(m);
}
