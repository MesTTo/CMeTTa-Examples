/* Purpose: Separate releasing a C handle from dropping engine state.
 * Owns resources: local handles and host storage are released before success;
 *   a failed assertion terminates this example process.
 * Guarantees: results are asserted [tested: make check; commit=6022c3f48b6dc64752c6e49cfe9d985c7ac7a4e9].
 * Open Obligations: None.
 */
#include "common.h"
int main(void)
{
    metta *m = open_engine();
    mt_space *space = mt_space_open(m, "&durable-name"); check("open", space != NULL);
    check("store fact", mt_add(space, mt_expr("item", 42))); mt_space_close(space);
    space = mt_space_open(m, "&durable-name"); check("reopen", space != NULL);
    check_answers("handle close preserves space", mt_atoms(space), "(item 42)");
    check("drop engine state", mt_space_drop(space)); mt_space_close(space);
    space = mt_space_open(m, "&durable-name"); check("open fresh", space != NULL);
    check("drop removed facts", mt_count(space) == 0);
    check("drop empty space", mt_space_drop(space)); mt_space_close(space);
    return done(m, "space_lifetime");
}
