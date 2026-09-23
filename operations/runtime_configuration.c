/* Purpose: configure the runtime. mt_open() takes a stack ceiling at boot,
 *   mt_limit() sets a budget later calls obey, and a live setting is an atom
 *   in the catalog space &metta, rewritten by removing the old value and
 *   adding the new, and read back by a match like any other fact.
 * Decides: this small host reserves a 256 MiB SWI stack.
 * Guarantees: the configured engine answers, the budget reads back, and the
 *   catalog setting moves from 7 to 3 [tested: make check; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

static int64_t display_rows(metta *m)
{
    return mt_one_int(mt_eval(m, E("match", mt_spaceref("&metta"),
                                   E("limit", "display-rows", V("n")), V("n"))));
}

int main(void)
{
    mt_config config = { .stack_limit = 256u * 1024u * 1024u };
    metta *m = mt_open(&config);
    require("boot with a stack ceiling", m != NULL);
    check_int("the configured engine answers", mt_one_int(mt_eval(m, E("+", 20, 22))), 42);

    require("set a budget", mt_limit(m, (mt_limits){ .inferences = 1000000 }));
    check("the budget reads back", mt_limits_of(m).inferences == 1000000);
    require("clear it", mt_limit(m, (mt_limits){0}));

    mt_space *catalog = mt_catalog(m);
    require("a live setting", mt_add(catalog, E("limit", "display-rows", 7)));
    check_int("is language data", display_rows(m), 7);
    require("remove the old value", mt_del(catalog, E("limit", "display-rows", 7)));
    require("add the new", mt_add(catalog, E("limit", "display-rows", 3)));
    check_int("and the setting moved", display_rows(m), 3);
    return done(m);
}
