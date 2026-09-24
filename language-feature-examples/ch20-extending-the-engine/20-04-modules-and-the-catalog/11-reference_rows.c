/* Purpose: a live reference to another space's definitions. &reference-home
 *   holds a public module-answer calling an internal module-helper, and
 *   &self its own module-helper; a (from &reference-home) row makes the
 *   home's public heads callable here while their bodies keep calling the
 *   home's helper. HELPER is one body over lowering.h's operators, lowered
 *   into each space with that space's offset and compiled for C, so each
 *   call answers what C computes with the offset of the space whose
 *   definition runs. The home's data and equations stay home, the
 *   declaration and the properties travel, two equal home equations stay two
 *   answers until one is subtracted, and removing the row withdraws what it
 *   brought.
 * Guarantees: all thirteen claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"
#include "lowering.h"

#define HELPER(ADD, x, offset) ADD(x, offset)
enum { HOME_OFFSET = 1, SELF_OFFSET = 100, LATER = 9 };

static mt_atom *arrow(void) { return E("->", "Number", "Number"); }

/* What (get-property name) says about one property, read through case. */
static mt_answers *property(metta *m, const char *name, mt_atom *pattern, mt_atom *answer)
{
    return mt_eval(m, E("case", E("get-property", name), E(E(pattern, answer))));
}

int main(void)
{
    metta *m = open_engine();
    mt_space *home = mt_space_open(m, "&reference-home");
    require("open &reference-home", home != NULL);
    require("the helper is internal", mt_add(home, E("internal", "module-helper")));
    require("its type", mt_add(home, E(":", "module-helper", arrow())));
    require("the home's helper", mt_lower(home, (module-helper $x), HELPER(M_ADD, $x, 1)));
    require("the answer's type", mt_add(home, E(":", "module-answer", arrow())));
    require("the answer calls the helper", mt_add(home, E("=", E("module-answer", V("x")), E("module-helper", V("x")))));
    require("its doc", mt_add(home, E("@doc", "module-answer", E("@desc", T("Add one at home")))));
    require("home data", mt_add(home, E("home-data", "kept")));

    require("this space's helper is internal too", mt_add(m, E("internal", "module-helper")));
    require("its type", mt_add(m, E(":", "module-helper", arrow())));
    require("this space's helper", mt_lower(m, (module-helper $x), HELPER(M_ADD, $x, 100)));
    mt_atom *reference = E("from", mt_spaceref("&reference-home"));
    require("the reference row", mt_add(m, mt_keep(reference)));

    const int64_t x = 2;
    check_int("the home's answer calls the home's helper", mt_one_int(mt_eval(m, E("module-answer", x))),
              HELPER(C_ADD, x, HOME_OFFSET));
    check_int("this space's helper is its own", mt_one_int(mt_eval(m, E("module-helper", x))), HELPER(C_ADD, x, SELF_OFFSET));
    check_none("the home's data stays home", mt_match(m, E("home-data", V("x"))));
    check_none("and so do its equations", mt_match(m, E("=", E("module-answer", V("x")), V("body"))));
    check_answers("the declaration travels", mt_match(m, E(":", "module-answer", V("type"))), E(":", "module-answer", arrow()));

    check_answers("module-answer is public", property(m, "module-answer", E("visibility", V("v")), V("v")), "public");
    check_answers("module-helper is internal", property(m, "module-helper", E("visibility", V("v")), V("v")), "internal");
    check_answers("the origin is the home, with no equation line",
                  property(m, "module-answer", E("origin", V("home"), V("file"), V("line")), E(V("home"), V("line"))),
                  E(mt_spaceref("&reference-home"), -1));
    check_int("evalc calls the helper in its home", mt_one_int(mt_eval(m, E("evalc", E("module-helper", x), mt_spaceref("&reference-home")))),
              HELPER(C_ADD, x, HOME_OFFSET));

    mt_atom *later = E("=", E("module-later"), LATER);
    for (int i = 0; i < 2; i++) require("module-later at home", mt_add(home, mt_keep(later)));
    check_answers("two equal equations are two answers", mt_eval(m, E("module-later")), LATER, LATER);
    require("subtract one", mt_one_truth(mt_eval(m, E("subtract-atom", mt_spaceref("&reference-home"), mt_keep(later)))));
    check_answers("one is left", mt_eval(m, E("module-later")), LATER);
    mt_drop(later);

    require("remove the reference", mt_del(m, reference));
    check_answers("its definitions went with it", mt_eval(m, E("module-answer", x)), E("module-answer", x));
    check_none("and its copied declaration", mt_match(m, E(":", "module-answer", V("type"))));
    mt_space_close(home);
    return done(m);
}
