/* Purpose: a mutable cell is an atom C holds. new-state answers the cell,
 *   and the same atom, kept with mt_keep(), is passed to get-state and
 *   change-state! for every read and write.
 * Guarantees: the cell reads 0, takes 7 with True, and reads 7 [tested: make check;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    mt_atom *cell = mt_one(mt_eval(m, E("new-state", 0)));
    require("create a cell", cell != NULL);
    check_int("it starts at 0", mt_one_int(mt_eval(m, E("get-state", mt_keep(cell)))), 0);
    check_answers("change-state! answers True",
                  mt_eval(m, E("change-state!", mt_keep(cell), 7)), B(true));
    check_int("and it reads 7", mt_one_int(mt_eval(m, E("get-state", mt_keep(cell)))), 7);
    mt_drop(cell);
    return done(m);
}
