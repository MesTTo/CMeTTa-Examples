/* Purpose: nested imports resolve against the file that wrote them. root
 *   imports subdir/parent and second, and parent imports sibling beside it,
 *   so importing root alone brings in both functions, each answering the
 *   constant its fixture defines.
 * Guarantees: both claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"
#include "modules.h"

/* The constant each nested fixture's function answers. */
static const struct { const char *function; int64_t value; } reached[] = { { "from-sibling", 42 }, { "from-second", 7 } };

int main(void)
{
    metta *m = open_engine();
    import_fixture(m, "&self", "imports/relative/root");
    for (size_t i = 0; i < sizeof reached / sizeof *reached; i++)
        check_int(reached[i].function, mt_one_int(mt_eval(m, E(reached[i].function))), reached[i].value);
    return done(m);
}
