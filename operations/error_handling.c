/* Purpose: failures are statuses, like errno. A wrong read records MT_MISUSE
 *   and returns 0; a later success leaves the record standing until
 *   mt_clear(); an empty answer is no failure at all; a false assertion is an
 *   engine error that carries the engine's remedy and its authority.
 * Guarantees: each of those four holds [tested: make check; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    mt_atom *words = T("not an integer");
    mt_atom *number = N(42);
    mt_clear();
    check_int("a wrong read returns 0", mt_int(words), 0);
    check("and records MT_MISUSE with words", mt_error() == MT_MISUSE && mt_errmsg() != NULL);
    check_int("a right read returns its value", mt_int(number), 42);
    check("and leaves the earlier failure standing", mt_error() == MT_MISUSE);
    mt_clear();

    check_none("an empty answer is not a failure", mt_eval(m, S("Empty")));
    check("so nothing was recorded", mt_ok());

    mt_atom *verdict = mt_first(mt_eval(m, E("assertEqual", 1, 2)));
    check("a false assertion is an engine error", verdict == NULL && mt_error() == MT_ERROR);
    check("with the engine's remedy and its ground", mt_remedy() != NULL && mt_ground() != NULL);
    mt_clear();
    mt_drop(words);
    mt_drop(number);
    return done(m);
}
