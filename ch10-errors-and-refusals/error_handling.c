/* Purpose: failures are statuses, like errno. A wrong read records MT_MISUSE
 *   and returns 0; a later success leaves the record standing until
 *   mt_clear(); an empty answer is no failure at all; a false assertion is an
 *   engine error that carries the engine's remedy and its authority.
 * Guarantees: each of those four holds [tested 2026-09-27T00:35:58+10:00:
 *   make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    mt_atom *words = T("not an integer");
    mt_atom *number = N(42);
    mt_clear();
    assert(mt_int(words) == 0 && "a wrong read returns 0");
    assert(mt_error() == MT_MISUSE && mt_errmsg() != NULL && "and records MT_MISUSE with words");
    assert(mt_int(number) == 42 && "a right read returns its value");
    assert(mt_error() == MT_MISUSE && "and leaves the earlier failure standing");
    mt_clear();

    assert(!mt_first(mt_eval(m, S("Empty"))) && mt_ok() && "an empty answer is not a failure");
    assert(mt_ok() && "so nothing was recorded");

    mt_atom *verdict = mt_first(mt_eval(m, E("assertEqual", 1, 2)));
    assert(verdict == NULL && mt_error() == MT_ERROR && "a false assertion is an engine error");
    assert(mt_remedy() != NULL && mt_ground() != NULL && "with the engine's remedy and its ground");
    mt_clear();
    mt_drop(words);
    mt_drop(number);
    mt_close(m);
    return 0;
}
