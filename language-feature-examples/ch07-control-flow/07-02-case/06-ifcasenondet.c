/* Purpose: a condition with three answers. if-nondet and case-nondet test a
 *   superposition, so each answer takes its own arm; C maps the same array
 *   of booleans through ?: for the answers it expects.
 * Guarantees: both claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

static const bool CONDITIONS[] = { true, false, true };
enum { N_CONDITIONS = sizeof CONDITIONS / sizeof *CONDITIONS };

static mt_atom *conditions(void)
{
    mt_atom *kids[N_CONDITIONS];
    for (size_t i = 0; i < N_CONDITIONS; i++) kids[i] = B(CONDITIONS[i]);
    return mt_exprv(N_CONDITIONS, kids);
}

static void expected(mt_atom **want)
{
    for (size_t i = 0; i < N_CONDITIONS; i++) want[i] = S(CONDITIONS[i] ? "a" : "b");
}

int main(void)
{
    metta *m = open_engine();
    require("if-nondet", mt_add(m, E("=", E("if-nondet", V("y")), E("if", E("superpose", V("y")), "a", "b"))));
    require("case-nondet", mt_add(m, E("=", E("case-nondet", V("y")),
                                      E("case", E("superpose", V("y")), E(E(B(true), "a"), E(B(false), "b"))))));

    mt_atom *want[N_CONDITIONS];
    expected(want);
    check_list_("if takes an arm per answer", mt_all(mt_eval(m, E("if-nondet", conditions()))), N_CONDITIONS, want);
    expected(want);
    check_list_("so does case", mt_all(mt_eval(m, E("case-nondet", conditions()))), N_CONDITIONS, want);
    return done(m);
}
