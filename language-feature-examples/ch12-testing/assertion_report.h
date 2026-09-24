/* Purpose: a failed assertion's report as C reads and builds it, shared by
 *   03-assertion_difference and 04-assert_answers: catch turns the failure
 *   into (Error Ball Context), and the ball is (metta_assertion_failed Call
 *   Missing Excess), each bag a directed bag_minus or, where the door had no
 *   bag to compare, an unbound variable.
 * Assumes: common.h included first with MT_SHORTHAND.
 */
#ifndef ASSERTION_REPORT_H
#define ASSERTION_REPORT_H

/* The ball a failing goal raises, taken out of catch's error; NULL when the
   goal did not fail. */
static inline mt_atom *reported(metta *m, mt_atom *goal)
{
    mt_atom *error = mt_one(mt_eval(m, E("catch", goal))), *ball = NULL;
    if (error && mt_kind_of(error) == MT_EXPR && mt_len(error) == 3 && mt_kind_of(mt_at(error, 0)) == MT_SYMBOL &&
        strcmp(mt_name(mt_at(error, 0)), "Error") == 0)
        ball = mt_keep(mt_at(error, 1));
    mt_drop(error);
    return ball;
}

/* The report of a two-sided comparison of an actual bag with an expected
   one: what was wanted and never produced, and what was produced and never
   wanted. */
static inline mt_atom *two_sided(const mt_atom *call, const mt_atom *actual, const mt_atom *expected)
{
    return E("metta_assertion_failed", mt_keep(call), bag_minus(expected, actual), bag_minus(actual, expected));
}

/* A one-sided containment's report: the missing bag, and no excess bag at
   all, since an answer beyond the expectation is legal. */
static inline mt_atom *one_sided(const mt_atom *call, const mt_atom *actual, const mt_atom *expected)
{
    return E("metta_assertion_failed", mt_keep(call), bag_minus(expected, actual), V("excess"));
}
#endif
