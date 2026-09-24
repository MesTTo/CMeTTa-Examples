/* Purpose: the claim checks every example states its results through, the
 *   report its last line hands the twin lane, and the two MeTTa readings the
 *   twins share: the metatype an atom's kind has, and a bag difference.
 * Assumes: one runtime per process, opened by open_engine() and closed by
 *   done(); claims may be checked from attached worker threads.
 * Guarantees:
 *   - a claim that does not hold ends the process with a nonzero status, the
 *     claim's words and both sides of the comparison, whatever NDEBUG says
 *     [tested: make check-helpers; commit=4fe77404069bc1a630ecc9e7860856a1117a200c]
 *   - done() refuses a program that checked nothing, left an error unhandled
 *     or could not close the engine [tested: make check-helpers;
 *     commit=4fe77404069bc1a630ecc9e7860856a1117a200c]
 *   - done() refuses a program that leaves any block cmetta allocated for it
 *     on the thread that opened the engine unreleased once the engine has
 *     closed, which is every atom it made and did not drop; the count is
 *     zero for a program that releases everything [tested: make check,
 *     twin_lane_selftest.py's leak case; commit=4fe77404069bc1a630ecc9e7860856a1117a200c]
 *   - require() is a status the program needs in order to go on and never
 *     counts as a claim, so the lane's claim count is the comparisons a
 *     program made on values it computed [tested: make twins;
 *     commit=4fe77404069bc1a630ecc9e7860856a1117a200c]
 */
#ifndef EXAMPLES_COMMON_H
#define EXAMPLES_COMMON_H
#include <cmetta.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Boot the engine, or end the process saying why it would not boot. */
metta *open_engine(void);

/* One claim of the program, proved on a value it computed. */
void check(const char *claim, bool holds);
void check_int(const char *claim, int64_t got, int64_t want);
void check_real(const char *claim, double got, double want);
void check_text(const char *claim, const char *got, const char *want);

/* Equal up to a consistent renaming of variables, MeTTa's =alpha. TAKES both
   atoms, the way every door taking a fresh term does. */
void check_atom(const char *claim, mt_atom *got, mt_atom *want);

/* The same question as a truth value, for a condition with more in it:
   BORROWS the atom the program holds and TAKES the expectation built to ask
   about it, so alpha_equal(row, E("item", 1)) leaves nothing behind. */
bool alpha_equal(const mt_atom *got, mt_atom *want);

/* Exactly these answers, in this order, each alpha-equal to its expectation.
   CONSUMES the cursor and TAKES every expectation; the expectations coerce
   the way mt_expr's children do, so 1, 2.5, "sym" and atoms all work:

       check_answers("three values", mt_eval(m, E("superpose", E(1, 2, 3))),
                     1, 2, 3);                                              */
#define check_answers(claim, answers, ...)                                \
    check_answers_((claim), (answers), MT_NARG(__VA_ARGS__),              \
                   (mt_atom *[]){ MT_MAP(__VA_ARGS__) })
void check_answers_(const char *claim, mt_answers *answers, size_t count,
                    mt_atom **want);

/* The same claim over a list the program already holds, such as one it
   sorted with qsort(..., mt_order). TAKES the list and every expectation. */
#define check_list(claim, list, ...)                                      \
    check_list_((claim), (list), MT_NARG(__VA_ARGS__),                    \
                (mt_atom *[]){ MT_MAP(__VA_ARGS__) })
void check_list_(const char *claim, mt_list list, size_t count, mt_atom **want);

/* No answer at all. CONSUMES the cursor. */
void check_none(const char *claim, mt_answers *answers);

/* Exactly this one value, where a value that is Empty is no answer at all:
   the engine answers a top-level Empty with nothing, and inside an expression
   Empty stays data. CONSUMES the cursor and TAKES the value. */
void check_value(const char *claim, mt_answers *answers, mt_atom *value);

/* The metatype get-metatype answers for an atom of this kind: Symbol for a
   symbol and for a space, which the engine names by a symbol; Variable;
   Expression, the empty one included; Grounded for every value, True and
   False and texts among them [measured 2026-09-24: build/tools/original on
   True, "s", &self, (), 1.5]. A symbol the engine resolves to a built-in
   operation, such as +, is Grounded to the engine and a symbol to C. */
const char *metatype(const mt_atom *atom);

/* from less take, as a bag: walking from in order, an atom keeps its place
   unless take still holds an identical one to cancel it, identity being the
   standard order's, so 1 and 1.0 are distinct. This is the counted
   subtraction subtraction-atom makes, from which assertEqualToResult's
   verdict and a failed assertion's missing and excess bags are built
   [source: engine/metta/input_guards.pl, subtract_counted/3;
   commit=2803a3ecf877ad410ab19a9168e09096eb50ef5a]. Borrows both; the
   answer is owned. Time: n*m comparisons, n and m the two lengths. */
mt_atom *bag_minus(const mt_atom *from, const mt_atom *take);

/* A status the program needs before it can go on: a door that must succeed.
   Not a claim, and never counted as one. */
void require(const char *what, bool ok);

/* Close the engine and report: the claims proved, then what the lane compares
   against the original, which is the definitions this program made visible,
   the C operations it published and the atoms &self holds. */
#define done(runtime) done_((runtime), __FILE__)
int done_(metta *runtime, const char *file);
#endif
