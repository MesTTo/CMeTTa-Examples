/* Purpose: a failed assertion reports which answers differ. C computes each
 *   report itself, from the bags it knows the forms produce and expect: a
 *   two-sided comparison's missing and excess bags are the two directed
 *   bag_minus differences, so a repeated answer counts every time; a
 *   one-sided containment has no excess bag; assert has a verdict and no
 *   bags at all; and a permutation fails assertEqual with two empty bags,
 *   since the collapsed tuples differ while the bags agree. The call in each
 *   report is the atom C built and evaluated. The passing forms' verdicts
 *   are C's too.
 * Guarantees: all ten claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"
#include "assertion_report.h"

int main(void)
{
    metta *m = open_engine();
    const struct { const char *claim; mt_atom *call, *actual, *expected; bool one_sided; } failures[] = {
        { "both directions", E("assertEqual", E("+", 1, 1), 3), E(1 + 1), E(3), false },
        { "one direction", E("assertEqualToResult", E("superpose", E(1, 2)), E(1, 2, 3)), E(1, 2), E(1, 2, 3), false },
        { "a bag difference, not a set one", E("assertEqual", E("superpose", E("a", "a", "b")), E("superpose", E("a", "b", "b"))), E("a", "a", "b"), E("a", "b", "b"), false },
        { "a one-sided question", E("assertIncludes", E("superpose", E(1, 2)), E(7)), E(1, 2), E(7), true },
        { "a permutation", E("assertEqual", E("superpose", E(1, 2)), E("superpose", E(2, 1))), E(1, 2), E(2, 1), false },
        { "a message form", E("assertEqualMsg", E("+", 1, 2), 4, T("sums differ")), E(1 + 2), E(4), false },
    };
    for (size_t i = 0; i < sizeof failures / sizeof *failures; i++) {
        mt_atom *ball = reported(m, mt_keep(failures[i].call));
        require("the assertion failed", ball != NULL);
        check_atom(failures[i].claim, ball,
                   (failures[i].one_sided ? one_sided : two_sided)(failures[i].call, failures[i].actual, failures[i].expected));
        mt_drop(failures[i].call), mt_drop(failures[i].actual), mt_drop(failures[i].expected);
    }
    mt_atom *ball = reported(m, E("assert", E("==", 1, 2)));
    require("assert failed", ball != NULL);
    check_atom("a verdict carries no bags", ball, E("metta_assertion_failed", B(1 == 2), V("missing"), V("excess")));

    mt_atom *sorted = E(1, 2), *reversed = E(2, 1), *missing = bag_minus(reversed, sorted), *excess = bag_minus(sorted, reversed);
    check_answers("the evidence travels, the verdict does not move", mt_eval(m, E("assertEqual", E("+", 1, 2), E("-", 6, 3))), B(1 + 2 == 6 - 3));
    check_answers("bags equal in any order", mt_eval(m, E("assertEqualToResult", E("superpose", mt_keep(sorted)), mt_keep(reversed))),
                  B(mt_len(missing) == 0 && mt_len(excess) == 0));
    check_answers("a passing message form", mt_eval(m, E("assertEqualMsg", E("+", 1, 2), E("-", 6, 3), T("sums differ"))), B(1 + 2 == 6 - 3));
    mt_drop(sorted), mt_drop(reversed), mt_drop(missing), mt_drop(excess);
    return done(m);
}
