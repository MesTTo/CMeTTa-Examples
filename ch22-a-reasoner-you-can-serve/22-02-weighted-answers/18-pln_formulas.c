/* Purpose: PLN's formulas and the helpers they are built from, one call
 *   each, with C's model of each in pln.h computing what the call must
 *   answer: the value, or nothing where a division through /safe has no
 *   positive denominator, so a claim that the engine answers nothing is C's
 *   model having no value, and C reads it off the cursor where the original
 *   collapses it into (). The tuple helpers are the C operations they are
 *   named for: a count, a filter, a search, first occurrences, an insertion
 *   and a sort. Test2's report is built from C's own comparison, and which
 *   link types the symmetric rule's guard admits is pln.h's table.
 * Guarantees: all forty-two claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include "_fixtures/nars_truth.h"
#include "_fixtures/pln.h"
#include <math.h>

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

/* What C's model answers, or nothing where it has no value. CONSUMES
   answers, TAKES model. */
static void as_modelled(const char *claim, mt_answers *answers, mt_atom *model)
{
    if (model)
        assert(answers_are(answers, E(model)) && claim);
    else
        assert(!mt_first(answers) && mt_ok() && claim);
}

static mt_atom *number_or_none(bool defined, double v) { return defined ? mt_real(v) : NULL; }
static mt_atom *truth_or_none(bool defined, truth t) { return defined ? stv(t) : NULL; }


/* Without: every item but those equal to X, in order. */
static mt_atom *without(const int64_t *v, size_t n, int64_t x)
{
    int64_t *kept = malloc((n ? n : 1) * sizeof *kept);
    require("room", kept != NULL);
    size_t k = 0;
    for (size_t i = 0; i < n; i++)
        if (v[i] != x) kept[k++] = v[i];
    mt_atom *list = mt_array(k, kept);
    free(kept);
    return list;
}

static bool element_of(int64_t x, const int64_t *v, size_t n)
{
    for (size_t i = 0; i < n; i++)
        if (v[i] == x) return true;
    return false;
}

/* Unique: first occurrences, in order. */
static mt_atom *unique(const int64_t *v, size_t n)
{
    int64_t *seen = malloc((n ? n : 1) * sizeof *seen);
    require("room", seen != NULL);
    size_t k = 0;
    for (size_t i = 0; i < n; i++)
        if (!element_of(v[i], seen, k)) seen[k++] = v[i];
    mt_atom *list = mt_array(k, seen);
    free(seen);
    return list;
}

/* InsertSorted: X before the first element it is less than, else last. */
static mt_atom *insert_sorted(int64_t x, const int64_t *v, size_t n)
{
    int64_t *out = malloc((n + 1) * sizeof *out);
    require("room", out != NULL);
    size_t i = 0, k = 0;
    for (; i < n && !(x < v[i]); i++) out[k++] = v[i];
    out[k++] = x;
    for (; i < n; i++) out[k++] = v[i];
    mt_atom *list = mt_array(k, out);
    free(out);
    return list;
}

static int ascending(const void *a, const void *b)
{
    int64_t x = *(const int64_t *)a, y = *(const int64_t *)b;
    return (x > y) - (x < y);
}

/* InsertionSort, which is msort: ascending, duplicates kept. */
static mt_atom *sorted(const int64_t *v, size_t n)
{
    int64_t *out = malloc((n ? n : 1) * sizeof *out);
    require("room", out != NULL);
    for (size_t i = 0; i < n; i++) out[i] = v[i];
    qsort(out, n, sizeof *out, ascending);
    mt_atom *list = mt_array(n, out);
    free(out);
    return list;
}

/* Test2's report: both sides and the verdict, as data. */
static mt_atom *report(int64_t is, int64_t should) { return E(E("Is:", is), E("Should:", should), E("Passed:", B(is == should))); }

/* The guard on the conjunction rule: three positive base probabilities, and
   two conditionals within what the base rates allow. */
static bool conjunction_consistent(double As, double Bs, double Cs, double ACs, double BCs)
{
    return As > 0 && Bs > 0 && Cs > 0 && ACs <= Cs / As && BCs <= Cs / Bs;
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("lib_pln", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_pln")))));

    /* The division every formula goes through, and the two built on it. */
    double q = 0;
    bool defined = safe_div(1.0, 2.0, &q);
    as_modelled("/safe", mt_eval(m, E("/safe", 1.0, 2.0)), number_or_none(defined, q));
    defined = safe_div(1.0, 0.0, &q);
    as_modelled("/safe by zero answers nothing", mt_eval(m, E("/safe", 1.0, 0.0)), number_or_none(defined, q));
    assert(answers_are(mt_eval(m, E("negate", 0.25)), E(negate(0.25))) && "negate");
    defined = invert(4.0, &q);
    as_modelled("invert", mt_eval(m, E("invert", 4.0)), number_or_none(defined, q));
    defined = invert(0.0, &q);
    as_modelled("invert of zero answers nothing", mt_eval(m, E("invert", 0.0)), number_or_none(defined, q));

    /* The five-argument spellings. */
    static const bool all_true[] = { true, true, true, true, true }, one_false[] = { true, true, false, true, true };
    const bool *conjunctions[] = { all_true, one_false };
    for (size_t i = 0; i < 2; i++) {
        const bool *v = conjunctions[i];
        assert(answers_are(mt_eval(m, E("and5", B(v[0]), B(v[1]), B(v[2]), B(v[3]), B(v[4]))), E(B(v[0] && v[1] && v[2] && v[3] && v[4])))
               && "and5");
    }
    assert(answers_are(mt_eval(m, E("min5", 5, 3, 9, 1, 7)), E((int64_t)MIN5(5, 3, 9, 1, 7))) && "min5");

    /* The tuple helpers, each the C operation it names. */
    static const char *const letters[] = { "a", "b", "c" };
    assert(answers_are(mt_eval(m, E("TupleCount", E(letters[0], letters[1], letters[2]))), E((int64_t)(sizeof letters / sizeof *letters)))
           && "TupleCount");
    assert(answers_are(mt_eval(m, E("TupleCount", mt_unit())), E((int64_t)0)) && "TupleCount of ()");
    static const int64_t repeats[] = { 1, 2, 3, 2 }, three[] = { 1, 2, 3 }, twice[] = { 1, 2, 1, 3 };
    assert(answers_are(mt_eval(m, E("Without", mt_array(4, repeats), 2)), E(without(repeats, 4, 2))) && "Without");
    assert(answers_are(mt_eval(m, E("ElementOf", 2, mt_array(3, three))), E(B(element_of(2, three, 3)))) && "ElementOf a member");
    assert(answers_are(mt_eval(m, E("ElementOf", 9, mt_array(3, three))), E(B(element_of(9, three, 3)))) && "ElementOf a stranger");
    assert(answers_are(mt_eval(m, E("Unique", mt_array(4, twice), mt_unit())), E(unique(twice, 4))) && "Unique");

    static const int64_t ends[] = { 1, 5 }, shuffled[] = { 3, 1, 2 };
    assert(answers_are(mt_eval(m, E("InsertSorted", 3, mt_unit())), E(insert_sorted(3, NULL, 0))) && "InsertSorted into ()");
    assert(answers_are(mt_eval(m, E("InsertSorted", 3, mt_array(2, ends))), E(insert_sorted(3, ends, 2))) && "InsertSorted between");
    assert(answers_are(mt_eval(m, E("InsertSorted", 0, mt_array(2, ends))), E(insert_sorted(0, ends, 2))) && "InsertSorted first");
    assert(answers_are(mt_eval(m, E("InsertionSort", mt_array(3, shuffled), mt_unit())), E(sorted(shuffled, 3))) && "InsertionSort");

    assert(answers_are(mt_eval(m, E("Test2", 1, 1)), E(report(1, 1))) && "Test2 passing");
    assert(answers_are(mt_eval(m, E("Test2", 1, 2)), E(report(1, 2))) && "Test2 failing");

    static const double conjunction_cases[][5] = {
        { 0.5, 0.5, 0.5, 0.5, 0.5 }, { 0.0, 0.5, 0.5, 0.5, 0.5 }, { 0.5, 0.5, 0.1, 0.5, 0.5 } };
    for (size_t i = 0; i < 3; i++) {
        const double *p = conjunction_cases[i];
        assert(answers_are(mt_eval(m, E("Consistency_ImplicationImplicantConjunction", p[0], p[1], p[2], p[3], p[4])), E(B(conjunction_consistent(p[0], p[1], p[2], p[3], p[4]))))
               && "Consistency_ImplicationImplicantConjunction");
    }

    /* The evidence map through /safe: a confidence of one has no weight. */
    defined = pln_w2c(9.0, &q);
    as_modelled("Truth_w2c", mt_eval(m, E("Truth_w2c", 9.0)), number_or_none(defined, q));
    assert(fabs(mt_one_float(mt_eval(m, E("Truth_c2w", 0.9))) - 9.0) < 1.0e-9 && "Truth_c2w inverts w2c at 0.9");
    defined = pln_c2w(1.0, &q);
    as_modelled("Truth_c2w of certainty answers nothing", mt_eval(m, E("Truth_c2w", 1.0)), number_or_none(defined, q));

    /* The truth formulas. */
    truth t = { 0 };
    const truth half = { 0.5, 0.9 };
    assert(answers_are(mt_eval(m, E("Truth_Negation", stv((truth){ 0.8, 0.9 }))), E(stv(pln_negation((truth){ 0.8, 0.9 })))) && "Truth_Negation");
    defined = pln_revision((truth){ 1.0, 0.9 }, (truth){ 0.0, 0.9 }, &t);
    as_modelled("Truth_Revision", mt_eval(m, E("Truth_Revision", stv((truth){ 1.0, 0.9 }), stv((truth){ 0.0, 0.9 }))),
                truth_or_none(defined, t));
    assert(answers_are(mt_eval(m, E("Truth_SymmetricModusPonens", stv((truth){ 1.0, 0.9 }), stv((truth){ 1.0, 0.8 }))), E(stv(symmetric_modus_ponens((truth){ 1.0, 0.9 }, (truth){ 1.0, 0.8 }))))
           && "Truth_SymmetricModusPonens");
    static const char *const links[] = { "Similarity", "IntentionalSimilarity", "ExtensionalSimilarity", "Inheritance" };
    for (size_t i = 0; i < sizeof links / sizeof *links; i++)
        as_modelled(links[i], mt_eval(m, E("SymmetricModusPonensRuleGuard", links[i])), symmetric_link(links[i]) ? B(true) : NULL);
    assert(answers_are(mt_eval(m, E("Truth_inversion", stv(half), stv((truth){ 0.8, 0.9 }))), E(stv(inversion(half, (truth){ 0.8, 0.9 }))))
           && "Truth_inversion");
    defined = equivalence_to_implication(half, half, (truth){ 0.8, 0.9 }, &t);
    as_modelled("Truth_equivalenceToImplication",
                mt_eval(m, E("Truth_equivalenceToImplication", stv(half), stv(half), stv((truth){ 0.8, 0.9 }))),
                truth_or_none(defined, t));

    defined = transitive_similarity_strength(0.5, 0.5, 0.5, 0.5, 0.5, &q);
    as_modelled("TransitiveSimilarityStrength", mt_eval(m, E("TransitiveSimilarityStrength", 0.5, 0.5, 0.5, 0.5, 0.5)),
                number_or_none(defined, q));
    const truth weaker = { 0.5, 0.8 }, weakest = { 0.5, 0.7 };
    defined = transitive_similarity(half, half, half, weaker, weakest, &t);
    as_modelled("Truth_transitiveSimilarity",
                mt_eval(m, E("Truth_transitiveSimilarity", stv(half), stv(half), stv(half), stv(weaker), stv(weakest))),
                truth_or_none(defined, t));

    defined = simple_deduction_strength(0.5, 0.5, 0.5, 0.5, 0.5, &q);
    as_modelled("simpleDeductionStrength", mt_eval(m, E("simpleDeductionStrength", 0.5, 0.5, 0.5, 0.5, 0.5)),
                number_or_none(defined, q));
    defined = simple_deduction_strength(0.0, 0.5, 0.5, 0.5, 0.5, &q);
    as_modelled("an inconsistent deduction answers nothing",
                mt_eval(m, E("simpleDeductionStrength", 0.0, 0.5, 0.5, 0.5, 0.5)), number_or_none(defined, q));
    defined = evaluation_implication(half, half, half, half, half, &t);
    as_modelled("Truth_evaluationImplication",
                mt_eval(m, E("Truth_evaluationImplication", stv(half), stv(half), stv(half), stv(half), stv(half))),
                truth_or_none(defined, t));

    defined = pln_induction(half, half, half, half, half, &t);
    as_modelled("Truth_Induction takes five",
                mt_eval(m, E("Truth_Induction", stv(half), stv(half), stv(half), stv(half), stv(half))), truth_or_none(defined, t));
    defined = pln_abduction(half, half, half, half, half, &t);
    as_modelled("Truth_Abduction takes five",
                mt_eval(m, E("Truth_Abduction", stv(half), stv(half), stv(half), stv(half), stv(half))), truth_or_none(defined, t));
    mt_close(m);
    return 0;
}
