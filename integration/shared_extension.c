/* Purpose: load a separately compiled C plugin and invoke its callback.
 * Owns resources: mt_close releases the runtime's plugin reference.
 * Guarantees: plugin output and missing-library errors are checked [tested: make check; commit=6022c3f48b6dc64752c6e49cfe9d985c7ac7a4e9].
 * Open Obligations: None.
 */
#include "common.h"
int main(void)
{
    metta *m = open_engine();
    check("load shared extension", mt_extension(m, "build/plugins/arithmetic.so"));
    check_answers("plugin calls back into engine", mt_run(m, "!(plugin-triple 14)"), "42");
    check("missing DSO is an error", !mt_extension(m, "build/plugins/absent.so") && !mt_ok());
    check("missing path is named", mt_errmsg() && strstr(mt_errmsg(), "absent.so")); mt_clear();
    return done(m, "shared_extension");
}
