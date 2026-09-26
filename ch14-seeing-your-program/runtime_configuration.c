/* Purpose: configure the runtime. mt_open() takes a stack ceiling at boot,
 *   mt_limit() sets a budget later calls obey, and a live setting is an atom
 *   in the catalog space &metta, rewritten by removing the old value and
 *   adding the new, and read back by a match like any other fact.
 * Decides: this small host reserves a 256 MiB SWI stack.
 * Guarantees: the configured engine answers, the budget reads back, and the
 *   catalog setting moves from 7 to 3 [tested 2026-09-27T00:35:58+10:00:
 *   make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>

/* A door the program needs before it can go on: on refusal, say why and stop. */
#define require(what, ok) \
    ((ok) ? (void)0 : (fprintf(stderr, "%s: %s\n", (what), mt_errmsg()), exit(EXIT_FAILURE)))

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
    assert(mt_one_int(mt_eval(m, E("+", 20, 22))) == 42 && "the configured engine answers");

    require("set a budget", mt_limit(m, (mt_limits){ .inferences = 1000000 }));
    assert(mt_limits_of(m).inferences == 1000000 && "the budget reads back");
    require("clear it", mt_limit(m, (mt_limits){0}));

    mt_space *catalog = mt_catalog(m);
    require("a live setting", mt_add(catalog, E("limit", "display-rows", 7)));
    assert(display_rows(m) == 7 && "is language data");
    require("remove the old value", mt_del(catalog, E("limit", "display-rows", 7)));
    require("add the new", mt_add(catalog, E("limit", "display-rows", 3)));
    assert(display_rows(m) == 3 && "and the setting moved");
    mt_close(m);
    return 0;
}
