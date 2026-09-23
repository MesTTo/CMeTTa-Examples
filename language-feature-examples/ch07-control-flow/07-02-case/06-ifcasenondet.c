/* Purpose: a condition with three answers. if-nondet and case-nondet test a
 *   superposition, so each answer takes its own arm; C maps the same array
 *   of booleans through ?: for the answers it expects.
 * Guarantees: both claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
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
    require("if-nondet", mt_lower(m, (if-nondet $y), (if (superpose $y) a b)));
    require("case-nondet", mt_lower(m, (case-nondet $y), (case (superpose $y) ((True a) (False b)))));

    mt_atom *want[N_CONDITIONS];
    expected(want);
    check_list_("if takes an arm per answer", mt_all(mt_eval(m, E("if-nondet", conditions()))), N_CONDITIONS, want);
    expected(want);
    check_list_("so does case", mt_all(mt_eval(m, E("case-nondet", conditions()))), N_CONDITIONS, want);
    return done(m);
}
