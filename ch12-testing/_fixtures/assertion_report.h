/* Purpose: a failed assertion's report as C reads and builds it, shared by
 *   03-assertion_difference and 04-assert_answers: catch turns the failure
 *   into (Error Ball Context), and the ball is (metta_assertion_failed Call
 *   Missing Excess), each bag a directed bag_minus or, where the door had no
 *   bag to compare, an unbound variable.
 * Assumes: the includer defines MT_SHORTHAND before its first include.
 */
#ifndef ASSERTION_REPORT_H
#define ASSERTION_REPORT_H
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A door the program needs before it can go on: on refusal, say why and stop. */
#define require(what, ok) \
    ((ok) ? (void)0 : (fprintf(stderr, "%s: %s\n", (what), mt_errmsg()), exit(EXIT_FAILURE)))

/* from less take, as a bag: an atom of from keeps its place unless take
   still holds an identical one to cancel it, the counted subtraction
   subtraction-atom makes. Borrows both; the answer is owned.
   Time: n*m comparisons, n and m the two lengths. */
static inline mt_atom *bag_minus(const mt_atom *from, const mt_atom *take)
{
    size_t n = mt_len(from), m = mt_len(take), kept = 0;
    bool *spent = calloc(m + 1, sizeof *spent);
    mt_atom **left = malloc((n + 1) * sizeof *left);
    require("room for a bag", spent && left);
    for (size_t i = 0; i < n; i++) {
        size_t j = 0;
        while (j < m && (spent[j] || mt_compare(mt_at(from, i), mt_at(take, j)) != 0)) j++;
        if (j < m) spent[j] = true;
        else left[kept++] = mt_keep(mt_at(from, i));
    }
    mt_atom *out = mt_exprv(kept, left);
    free(spent), free(left);
    return out;
}

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
