/* Purpose: the step whose answer is not wanted, C's (void). nop evaluates
 *   every argument and answers the unit, so a write inside it lands and one
 *   unit comes back per branch, where empty answers nothing. The writes go
 *   to spaces C opens by name and reads back itself, mt_count being
 *   space-atom-count.
 * Guarantees: all eleven claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    mt_space *log = mt_space_open(m, "&log"), *drained = mt_space_open(m, "&drained");
    require("open &log and &drained", log && drained);

    check_answers("(nop)", mt_eval(m, E("nop")), mt_unit());
    check_answers("(nop 1)", mt_eval(m, E("nop", 1)), mt_unit());
    check_answers("(nop 1 2 3)", mt_eval(m, E("nop", 1, 2, 3)), mt_unit());

    check_answers("the writes inside land",
                  mt_eval(m, E("nop", E("add-atom", mt_spaceref("&log"), E("seen", 1)),
                               E("add-atom", mt_spaceref("&log"), E("seen", 2)))), mt_unit());
    check_answers("and C reads them back", mt_eval(log, E("match", mt_spaceref("&log"), E("seen", V("n")), V("n"))), 1, 2);

    check_list("nop answers one unit", mt_all(mt_eval(m, E("nop", 1))), mt_unit());
    check_none("empty answers nothing", mt_eval(m, E("empty")));

    check_answers("three branches, three units",
                  mt_eval(m, E("nop", E("let", V("x"), E("superpose", E(1, 2, 3)),
                                        E("add-atom", mt_spaceref("&drained"), E("x", V("x")))))),
                  mt_unit(), mt_unit(), mt_unit());
    check_int("and all three writes landed", (int64_t)mt_count(drained), 3);

    check_answers("the unit is not add-atom's answer",
                  mt_eval(m, E("==", E("nop", 1), E("add-atom", mt_spaceref("&log"), E("seen", 3)))), B(false));
    check_answers("but every nop's is the same", mt_eval(m, E("==", E("nop", 1), E("nop", 2))), B(true));
    mt_space_close(log);
    mt_space_close(drained);
    return done(m);
}
