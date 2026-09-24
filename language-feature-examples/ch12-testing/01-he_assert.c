/* Purpose: lib_he's assertions and the engine's bag forms, each verdict held
 *   against C's own: equal sums by C's +, alpha-equality by mt_alpha_eq,
 *   lib_he's ToResult as one comparison per answer, a bag containment as an
 *   empty bag_minus of the expected from the produced, and bag equality as
 *   containment both ways.
 * Guarantees: all twelve claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

static bool contains_all(const mt_atom *produced, const mt_atom *expected)
{
    mt_atom *missing = bag_minus(expected, produced);
    bool holds = mt_len(missing) == 0;
    mt_drop(missing);
    return holds;
}

/* Equal as bags: each contains the other. */
static bool same_bag(const mt_atom *a, const mt_atom *b) { return contains_all(a, b) && contains_all(b, a); }

int main(void)
{
    metta *m = open_engine();
    require("import lib_he", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_he")))));
    check_answers("equal sums", mt_eval(m, E("assertEqual", E("+", 1, 2), E("-", 6, 3))), B(1 + 2 == 6 - 3));
    mt_atom *h_xy = E("h", V("x"), V("y")), *h_ab = E("h", V("a"), V("b"));
    check_answers("alpha-equal terms", mt_eval(m, E("assertAlphaEqual", mt_keep(h_xy), mt_keep(h_ab))), B(mt_alpha_eq(h_xy, h_ab)));
    mt_atom *sum_xy = E("+", V("x"), V("y")), *sum_ab = E("+", V("a"), V("b"));
    check_answers("alpha-equal quoted sums", mt_eval(m, E("assertAlphaEqual", E("quote", mt_keep(sum_xy)), E("quote", mt_keep(sum_ab)))), B(mt_alpha_eq(sum_xy, sum_ab)));
    check_answers("ToResult compares an answer", mt_eval(m, E("assertEqualToResult", E("+", 1, 2), 3)), B(1 + 2 == 3));
    const int64_t twice[] = { 1, 1 };
    check_answers("once per answer", mt_eval(m, E("collapse", E("assertEqualToResult", E("superpose", E(twice[0], twice[1])), 1))), E(B(twice[0] == 1), B(twice[1] == 1)));
    mt_atom *x = E(V("x")), *y = E(V("y"));
    require("(= (adder) ($x))", mt_add(m, E("=", E("adder"), mt_keep(x))));
    check_answers("alpha-equal results", mt_eval(m, E("assertAlphaEqualToResult", E("adder"), mt_keep(y))), B(mt_alpha_eq(x, y)));
    mt_atom *produced = E(1, 2, 3), *wanted[] = { E(2), E(2, 3) };
    for (size_t i = 0; i < 2; i++) {
        check_answers("the bag contains the expected", mt_eval(m, E("assertIncludes", E("superpose", mt_keep(produced)), mt_keep(wanted[i]))), B(contains_all(produced, wanted[i])));
        mt_drop(wanted[i]);
    }
    check_answers("a message form", mt_eval(m, E("assertEqualMsg", E("+", 1, 2), E("-", 6, 3), T("sums differ"))), B(1 + 2 == 6 - 3));
    check_answers("and its alpha twin", mt_eval(m, E("assertAlphaEqualMsg", mt_keep(h_xy), mt_keep(h_ab), T("not alpha equal"))), B(mt_alpha_eq(h_xy, h_ab)));
    mt_atom *answers = E(1 + 2), *expected = E(3);
    check_answers("a bag message form", mt_eval(m, E("assertEqualToResultMsg", E("+", 1, 2), mt_keep(expected), T("not the expected result"))), B(same_bag(answers, expected)));
    mt_drop(answers), mt_drop(expected);
    check_answers("and its alpha twin", mt_eval(m, E("assertAlphaEqualToResultMsg", E("adder"), E(mt_keep(y)), T("not alpha equal"))), B(mt_alpha_eq(x, y)));
    mt_atom *held[] = { h_xy, h_ab, sum_xy, sum_ab, x, y, produced };
    for (size_t i = 0; i < 7; i++) mt_drop(held[i]);
    return done(m);
}
