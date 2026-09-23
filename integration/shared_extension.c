/* Purpose: a separately compiled plugin extends the seat. mt_extension()
 *   loads build/plugins/arithmetic.so, calls its mt_extension_init(), and
 *   the function it registers is called like any other; a missing plugin is
 *   refused by name.
 * Owns resources: the runtime keeps the plugin loaded until exit.
 * Guarantees: (plugin-triple 14) is 42, and a missing path is named in the
 *   refusal [tested: make check; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    require("load the plugin", mt_extension(m, "build/plugins/arithmetic.so"));
    check_int("its function answers", mt_one_int(mt_eval(m, E("plugin-triple", 14))), 42);
    mt_clear();
    check("a missing plugin is refused", !mt_extension(m, "build/plugins/absent.so") && !mt_ok());
    check("naming the path", mt_errmsg() && strstr(mt_errmsg(), "absent.so") != NULL);
    mt_clear();
    return done(m);
}
