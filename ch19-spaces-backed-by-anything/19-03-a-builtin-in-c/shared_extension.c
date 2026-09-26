/* Purpose: a separately compiled plugin extends the seat. mt_extension()
 *   loads _fixtures/arithmetic_extension.so from beside the program, calls
 *   its mt_extension_init(), and the function it registers is called like
 *   any other; a missing plugin is refused by name.
 * Build: cc -shared -fPIC _fixtures/arithmetic_extension.c $(pkg-config
 *   --cflags --libs cmetta) -o _fixtures/arithmetic_extension.so, then
 *   cc shared_extension.c $(pkg-config --cflags --libs cmetta).
 * Owns resources: the runtime keeps the plugin loaded until exit.
 * Guarantees: (plugin-triple 14) is 42, and a missing path is named in the
 *   refusal [tested 2026-09-27T00:35:58+10:00:
 *   make -C extensions/cmetta corpus-check].
 */
#define _POSIX_C_SOURCE 200809L
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <limits.h>

/* A door the program needs before it can go on: on refusal, say why and stop. */
#define require(what, ok) \
    ((ok) ? (void)0 : (fprintf(stderr, "%s: %s\n", (what), mt_errmsg()), exit(EXIT_FAILURE)))

int main(int argc, char **argv)
{
    (void)argc;
    /* The plugin sits in the _fixtures/ beside the program, wherever it runs. */
    char here[PATH_MAX], plugin[PATH_MAX];
    snprintf(here, sizeof here, "%s", argv[0]);
    snprintf(plugin, sizeof plugin, "%s/_fixtures/arithmetic_extension.so", dirname(here));
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("load the plugin", mt_extension(m, plugin));
    assert(mt_one_int(mt_eval(m, E("plugin-triple", 14))) == 42 && "its function answers");
    mt_clear();
    assert(!mt_extension(m, "_fixtures/absent.so") && !mt_ok() && "a missing plugin is refused");
    assert(mt_errmsg() && strstr(mt_errmsg(), "absent.so") != NULL && "naming the path");
    mt_clear();
    mt_close(m);
    return 0;
}
