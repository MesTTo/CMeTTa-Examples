/* Purpose: use the same consumer source through direct, pkg-config and CMake links.
 * Owns resources: closes the embedded runtime after checking the answer.
 * Guarantees: consumer builds execute this result check [tested: make check-consumers; commit=WORKTREE].
 * Open Obligations: None.
 */
#include "common.h"
int main(void)
{
    metta *m = open_engine();
    check_answers("installed runtime answers", mt_eval(m, mt_expr("+", 20, 22)), "42");
    check("runtime version matches header", strcmp(mt_version(), MT_VERSION) == 0);
    return done(m, "installed_consumer");
}
