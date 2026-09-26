/* Purpose: the same program built three ways against an installed cmetta:
 *   directly, through pkg-config and through CMake.
 * Owns resources: the runtime, closed after the answer is checked.
 * Guarantees: an installed library boots its installed engine and answers
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    assert(mt_one_int(mt_eval(m, E("+", 20, 22))) == 42 && "the installed runtime answers");
    assert(strcmp(mt_version(), MT_VERSION) == 0 && "the library is the header's version");
    mt_close(m);
    return 0;
}
