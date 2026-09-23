/* Purpose: Keep collected answers unchanged after later writes.
 * Owns resources: local C handles are released before exit; a failed check
 *   terminates the example process.
 * Guarantees: the assertions below hold [tested: make check; commit=WORKTREE].
 * Open Obligations: None.
 */
#include "common.h"
int main(void)
{
    metta *m = open_engine();
    check("initial item", mt_add(m, mt_expr("item", 1)));
    mt_list snapshot = mt_all(mt_atoms(m));
    check("second item", mt_add(m, mt_expr("item", 2)));
    check("snapshot has one row", snapshot.len == 1);
    check_atom("original row retained", snapshot.items[0], "(item 1)");
    check("live space has two rows", mt_count(m) == 2);
    mt_list_free(snapshot);
    return done(m, "snapshot");
}

