/* Purpose: lib_he's assertions and the engine's bag forms, each verdict held
 *   against C's own: equal sums by C's +, alpha-equality by mt_alpha_eq,
 *   lib_he's ToResult as one comparison per answer, a bag containment as an
 *   empty bag_minus of the expected from the produced, and bag equality as
 *   containment both ways.
 * Guarantees: all twelve claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>

/* A door the program needs before it can go on: on refusal, say why and stop. */
#define require(what, ok) \
    ((ok) ? (void)0 : (fprintf(stderr, "%s: %s\n", (what), mt_errmsg()), exit(EXIT_FAILURE)))

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
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_he", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_he")))));
    assert(answers_are(mt_eval(m, E("assertEqual", E("+", 1, 2), E("-", 6, 3))), E(B(1 + 2 == 6 - 3))) && "equal sums");
    mt_atom *h_xy = E("h", V("x"), V("y")), *h_ab = E("h", V("a"), V("b"));
    assert(answers_are(mt_eval(m, E("assertAlphaEqual", mt_keep(h_xy), mt_keep(h_ab))), E(B(mt_alpha_eq(h_xy, h_ab)))) && "alpha-equal terms");
    mt_atom *sum_xy = E("+", V("x"), V("y")), *sum_ab = E("+", V("a"), V("b"));
    assert(answers_are(mt_eval(m, E("assertAlphaEqual", E("quote", mt_keep(sum_xy)), E("quote", mt_keep(sum_ab)))), E(B(mt_alpha_eq(sum_xy, sum_ab)))) && "alpha-equal quoted sums");
    assert(answers_are(mt_eval(m, E("assertEqualToResult", E("+", 1, 2), 3)), E(B(1 + 2 == 3))) && "ToResult compares an answer");
    const int64_t twice[] = { 1, 1 };
    assert(answers_are(mt_eval(m, E("collapse", E("assertEqualToResult", E("superpose", E(twice[0], twice[1])), 1))), E(E(B(twice[0] == 1), B(twice[1] == 1)))) && "once per answer");
    mt_atom *x = E(V("x")), *y = E(V("y"));
    require("(= (adder) ($x))", mt_add(m, E("=", E("adder"), mt_keep(x))));
    assert(answers_are(mt_eval(m, E("assertAlphaEqualToResult", E("adder"), mt_keep(y))), E(B(mt_alpha_eq(x, y)))) && "alpha-equal results");
    mt_atom *produced = E(1, 2, 3), *wanted[] = { E(2), E(2, 3) };
    for (size_t i = 0; i < 2; i++) {
        assert(answers_are(mt_eval(m, E("assertIncludes", E("superpose", mt_keep(produced)), mt_keep(wanted[i]))), E(B(contains_all(produced, wanted[i])))) && "the bag contains the expected");
        mt_drop(wanted[i]);
    }
    assert(answers_are(mt_eval(m, E("assertEqualMsg", E("+", 1, 2), E("-", 6, 3), T("sums differ"))), E(B(1 + 2 == 6 - 3))) && "a message form");
    assert(answers_are(mt_eval(m, E("assertAlphaEqualMsg", mt_keep(h_xy), mt_keep(h_ab), T("not alpha equal"))), E(B(mt_alpha_eq(h_xy, h_ab)))) && "and its alpha twin");
    mt_atom *answers = E(1 + 2), *expected = E(3);
    assert(answers_are(mt_eval(m, E("assertEqualToResultMsg", E("+", 1, 2), mt_keep(expected), T("not the expected result"))), E(B(same_bag(answers, expected)))) && "a bag message form");
    mt_drop(answers), mt_drop(expected);
    assert(answers_are(mt_eval(m, E("assertAlphaEqualToResultMsg", E("adder"), E(mt_keep(y)), T("not alpha equal"))), E(B(mt_alpha_eq(x, y)))) && "and its alpha twin");
    mt_atom *held[] = { h_xy, h_ab, sum_xy, sum_ab, x, y, produced };
    for (size_t i = 0; i < 7; i++) mt_drop(held[i]);
    mt_close(m);
    return 0;
}
