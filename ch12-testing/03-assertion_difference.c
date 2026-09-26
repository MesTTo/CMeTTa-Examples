/* Purpose: a failed assertion reports which answers differ. C computes each
 *   report itself, from the bags it knows the forms produce and expect: a
 *   two-sided comparison's missing and excess bags are the two directed
 *   bag_minus differences, so a repeated answer counts every time; a
 *   one-sided containment has no excess bag; assert has a verdict and no
 *   bags at all; and a permutation fails assertEqual with two empty bags,
 *   since the collapsed tuples differ while the bags agree. The call in each
 *   report is the atom C built and evaluated. The passing forms' verdicts
 *   are C's too.
 * Guarantees: all ten claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include "_fixtures/assertion_report.h"

/* Whether a list holds exactly the children of want, in order, each equal
   up to renaming variables; takes both, and shows them when they differ. */
static inline bool list_is(mt_list got, mt_atom *want)
{
    bool holds = mt_ok() && want && got.len == mt_len(want);
    for (size_t i = 0; holds && i < got.len; i++)
        holds = mt_alpha_eq(got.items[i], mt_at(want, i));
    if (!holds) {
        fprintf(stderr, "  got");
        for (size_t i = 0; i < got.len; i++) fprintf(stderr, " %s", mt_show(got.items[i]));
        fprintf(stderr, "\n  want %s\n", want ? mt_show(want) : "nothing");
    }
    mt_list_free(got);
    mt_drop(want);
    return holds;
}

/* Whether a query answers exactly the children of want; takes both. */
static inline bool answers_are(mt_answers *answers, mt_atom *want)
{
    return list_is(mt_all(answers), want);
}

/* Whether an atom is want up to renaming variables; takes both, and shows
   them when they differ. */
static inline bool atom_is(mt_atom *got, mt_atom *want)
{
    bool holds = got && want && mt_alpha_eq(got, want);
    if (!holds) fprintf(stderr, "  got %s\n  want %s\n", got ? mt_show(got) : "nothing",
                        want ? mt_show(want) : "nothing");
    mt_drop(got);
    mt_drop(want);
    return holds;
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
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
        assert(atom_is(ball, (failures[i].one_sided ? one_sided : two_sided)(failures[i].call, failures[i].actual, failures[i].expected))
               && failures[i].claim);
        mt_drop(failures[i].call), mt_drop(failures[i].actual), mt_drop(failures[i].expected);
    }
    mt_atom *ball = reported(m, E("assert", E("==", 1, 2)));
    require("assert failed", ball != NULL);
    assert(atom_is(ball, E("metta_assertion_failed", B(1 == 2), V("missing"), V("excess"))) && "a verdict carries no bags");

    mt_atom *sorted = E(1, 2), *reversed = E(2, 1), *missing = bag_minus(reversed, sorted), *excess = bag_minus(sorted, reversed);
    assert(answers_are(mt_eval(m, E("assertEqual", E("+", 1, 2), E("-", 6, 3))), E(B(1 + 2 == 6 - 3))) && "the evidence travels, the verdict does not move");
    assert(answers_are(mt_eval(m, E("assertEqualToResult", E("superpose", mt_keep(sorted)), mt_keep(reversed))), E(B(mt_len(missing) == 0 && mt_len(excess) == 0)))
           && "bags equal in any order");
    assert(answers_are(mt_eval(m, E("assertEqualMsg", E("+", 1, 2), E("-", 6, 3), T("sums differ"))), E(B(1 + 2 == 6 - 3))) && "a passing message form");
    mt_drop(sorted), mt_drop(reversed), mt_drop(missing), mt_drop(excess);
    mt_close(m);
    return 0;
}
