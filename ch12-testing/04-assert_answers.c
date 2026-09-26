/* Purpose: the reporting doors under the assert family. assert-answers
 *   takes a verdict, the call to report and two bags, and C computes what it
 *   reports from the same bags: nothing for a true verdict, and for a false
 *   one the missing and excess bag_minus differences; assert-includes-answers
 *   reports no excess bag. A form written over the door, assert-sorted, is
 *   held against C's own sort with mt_order: sorted answers pass, and an
 *   unsorted bag fails with two empty bags, since sorting keeps the bag.
 * Guarantees: all ten claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
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

/* A bag sorted into the standard order, as sort-atom sorts it. */
static mt_atom *sorted_bag(const mt_atom *bag)
{
    mt_list items = { malloc((mt_len(bag) + 1) * sizeof(mt_atom *)), mt_len(bag) };
    require("room to sort", items.items != NULL);
    for (size_t i = 0; i < items.len; i++) items.items[i] = mt_keep(mt_at(bag, i));
    qsort(items.items, items.len, sizeof *items.items, mt_order);
    mt_atom *out = mt_exprv(items.len, items.items);
    free(items.items);
    return out;
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    mt_atom *check1 = E("my-check", 1), *check2 = E("my-check", 2);
    assert(answers_are(mt_eval(m, E("assert-answers", B(true), mt_keep(check1), E(1, 2), E(1, 2))), E(B(true))) && "a true verdict");
    assert(answers_are(mt_eval(m, E("assert-answers", E("==", 1, 1), mt_keep(check1), E(1), E(1))), E(B(1 == 1))) && "a verdict that evaluates");
    const struct { const char *claim; mt_atom *call, *actual, *expected; } falses[] = {
        { "the two directed differences", mt_keep(check1), E(1, 2), E(1, 3) },
        { "counted as bags", E("checked", E("f", 1)), E("a", "a", "b"), E("a", "b", "b") },
    };
    for (size_t i = 0; i < 2; i++) {
        mt_atom *ball = reported(m, E("assert-answers", B(false), mt_keep(falses[i].call), mt_keep(falses[i].actual), mt_keep(falses[i].expected)));
        require("the door raised", ball != NULL);
        assert(atom_is(ball, two_sided(falses[i].call, falses[i].actual, falses[i].expected)) && falses[i].claim);
        mt_drop(falses[i].call), mt_drop(falses[i].actual), mt_drop(falses[i].expected);
    }
    assert(answers_are(mt_eval(m, E("assert-includes-answers", B(true), mt_keep(check2), E(1, 2, 3), E(1, 2))), E(B(true))) && "a true containment");
    mt_atom *actual = E(1, 2), *expected = E(7);
    mt_atom *ball = reported(m, E("assert-includes-answers", B(false), mt_keep(check2), mt_keep(actual), mt_keep(expected)));
    require("the door raised", ball != NULL);
    assert(atom_is(ball, one_sided(check2, actual, expected)) && "a one-sided report");

    mt_atom *equal = E("assertEqual", E("+", 1, 1), 3), *sum = E(1 + 1), *three = E(3);
    ball = reported(m, mt_keep(equal));
    require("assertEqual failed", ball != NULL);
    assert(atom_is(ball, two_sided(equal, sum, three)) && "assertEqual reports through the two-sided door");
    mt_atom *includes = E("assertIncludes", E("superpose", mt_keep(actual)), mt_keep(expected));
    ball = reported(m, mt_keep(includes));
    require("assertIncludes failed", ball != NULL);
    assert(atom_is(ball, one_sided(includes, actual, expected)) && "assertIncludes through the one-sided one");

    require("(: assert-sorted (-> Atom Bool))", mt_add(m, E(":", "assert-sorted", E("->", "Atom", "Bool"))));
    require("define assert-sorted", mt_add(m, E("=", E("assert-sorted", V("expression")),
        E("let*", E(E(V("answers"), E("collapse", V("expression"))), E(V("sorted"), E("sort-atom", V("answers")))),
          E("assert-answers", E("==", V("answers"), V("sorted")), E("assert-sorted", V("expression")), V("answers"), V("sorted"))))));
    mt_atom *in_order = E(1, 2, 3), *in_order_sorted = sorted_bag(in_order);
    assert(answers_are(mt_eval(m, E("assert-sorted", E("superpose", mt_keep(in_order)))), E(B(mt_eq(in_order, in_order_sorted)))) && "a sorted bag passes");
    mt_atom *shuffled = E(3, 1, 2), *call = E("assert-sorted", E("superpose", mt_keep(shuffled))), *shuffled_sorted = sorted_bag(shuffled);
    ball = reported(m, mt_keep(call));
    require("assert-sorted failed", ball != NULL);
    assert(atom_is(ball, two_sided(call, shuffled, shuffled_sorted)) && "an unsorted one fails with its bag intact");
    mt_atom *held[] = { check1, check2, actual, expected, equal, sum, three, includes, in_order, in_order_sorted, shuffled, call, shuffled_sorted };
    for (size_t i = 0; i < sizeof held / sizeof *held; i++) mt_drop(held[i]);
    mt_close(m);
    return 0;
}
