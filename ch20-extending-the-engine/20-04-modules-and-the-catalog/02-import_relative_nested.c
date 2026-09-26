/* Purpose: nested imports resolve against the file that wrote them. root
 *   imports subdir/parent and second, and parent imports sibling beside it,
 *   so importing root alone brings in both functions, each answering the
 *   constant its fixture defines.
 * Guarantees: both claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include "_fixtures/modules.h"

/* The constant each nested fixture's function answers. */
static const struct { const char *function; int64_t value; } reached[] = { { "from-sibling", 42 }, { "from-second", 7 } };

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    import_fixture(m, "&self", "imports/relative/root");
    for (size_t i = 0; i < sizeof reached / sizeof *reached; i++)
        assert(mt_one_int(mt_eval(m, E(reached[i].function))) == reached[i].value && reached[i].function);
    mt_close(m);
    return 0;
}
