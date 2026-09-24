/* Purpose: mt_all() is a snapshot. The collected list is the program's own,
 *   so a write after collecting changes the space and not the list.
 * Guarantees: the snapshot keeps one row while the space grows to two
 *   [tested: make check; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    require("store (item 1)", mt_add(m, E("item", 1)));
    mt_list snapshot = mt_all(mt_atoms(m));
    require("store (item 2)", mt_add(m, E("item", 2)));

    check_int("the snapshot still holds one row", (int64_t)snapshot.len, 1);
    check("and it is (item 1)", snapshot.len == 1 && alpha_equal(snapshot.items[0], E("item", 1)));
    check_int("while the space holds two", (int64_t)mt_count(m), 2);
    mt_list_free(snapshot);
    return done(m);
}
