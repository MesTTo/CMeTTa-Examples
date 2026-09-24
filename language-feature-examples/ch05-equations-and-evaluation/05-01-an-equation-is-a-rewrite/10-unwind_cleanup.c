/* Purpose: cleanup as an equation. on-unwind runs its handler once when the
 *   wrapped computation fails or is cut, and never when it succeeds; the
 *   handler is an equation that records the outcome as a fact, and C reads
 *   the facts to see which exits ran it.
 * Guarantees: success leaves no note, failure notes (fail), and a caller's
 *   once notes (!) [tested: make twins; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    /* (= (unwind-note $home $outcome) (add-atom $home (unwound $outcome))) */
    require("define the handler", mt_add(m, E("=", E("unwind-note", V("home"), V("outcome")),
                                              E("add-atom", V("home"), E("unwound", V("outcome"))))));
    mt_atom *note = E("unwind-note", "&self");
    mt_atom *unwound = E("unwound", V("outcome"));

    check_int("a deterministic success skips cleanup",
              mt_one_int(mt_eval(m, E("on-unwind", 7, mt_keep(note)))), 7);
    check_none("so nothing was noted", mt_match(m, mt_keep(unwound)));

    check_none("a failure answers nothing", mt_eval(m, E("on-unwind", E("superpose", mt_unit()), mt_keep(note))));
    check_answers("and notes (fail)", mt_match(m, mt_keep(unwound)), E("unwound", E("fail")));
    require("clear the note", mt_del(m, E("unwound", E("fail"))));

    check_int("a caller's once cuts after the first answer",
              mt_one_int(mt_eval(m, E("once", E("on-unwind", E("superpose", E(1, 2)), note)))), 1);
    check_answers("and notes the cut", mt_match(m, unwound), E("unwound", E("!")));
    return done(m);
}
