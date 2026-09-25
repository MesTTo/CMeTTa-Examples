/* Purpose: the Empty branch is the one a key with no answers takes. wu's key
 *   is (empty), so it takes Empty; wu2's key is (f), a C function answering
 *   42, so it takes the 42 branch instead.
 * Guarantees: both claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

static mt_status f(mt_call *call, void *user)
{
    (void)user;
    return mt_answer(call, N(42));
}

int main(void)
{
    metta *m = open_engine();
    require("publish f", mt_def(m, (mt_op){ .name = "f", .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = f }));
    require("wu", mt_add(m, E("=", E("wu"), E("case", E("empty"), E(E(1, 2), E("Empty", 42))))));
    require("wu2", mt_add(m, E("=", E("wu2"), E("case", E("f"), E(E(42, "ok"), E("Empty", "nok"))))));
    check_answers("a key with no answers takes Empty", mt_eval(m, E("wu")), 42);
    check_answers("a key that answers takes its own branch", mt_eval(m, E("wu2")), "ok");
    return done(m);
}
