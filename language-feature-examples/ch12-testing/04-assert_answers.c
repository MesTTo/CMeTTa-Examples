/* Purpose: the reporting doors under the assert family. assert-answers
 *   takes a verdict, the call to report and two bags, and C computes what it
 *   reports from the same bags: nothing for a true verdict, and for a false
 *   one the missing and excess bag_minus differences; assert-includes-answers
 *   reports no excess bag. A form written over the door, assert-sorted, is
 *   held against C's own sort with mt_order: sorted answers pass, and an
 *   unsorted bag fails with two empty bags, since sorting keeps the bag.
 * Guarantees: all ten claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include "assertion_report.h"

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
    metta *m = open_engine();
    mt_atom *check1 = E("my-check", 1), *check2 = E("my-check", 2);
    check_answers("a true verdict", mt_eval(m, E("assert-answers", B(true), mt_keep(check1), E(1, 2), E(1, 2))), B(true));
    check_answers("a verdict that evaluates", mt_eval(m, E("assert-answers", E("==", 1, 1), mt_keep(check1), E(1), E(1))), B(1 == 1));
    const struct { const char *claim; mt_atom *call, *actual, *expected; } falses[] = {
        { "the two directed differences", mt_keep(check1), E(1, 2), E(1, 3) },
        { "counted as bags", E("checked", E("f", 1)), E("a", "a", "b"), E("a", "b", "b") },
    };
    for (size_t i = 0; i < 2; i++) {
        mt_atom *ball = reported(m, E("assert-answers", B(false), mt_keep(falses[i].call), mt_keep(falses[i].actual), mt_keep(falses[i].expected)));
        require("the door raised", ball != NULL);
        check_atom(falses[i].claim, ball, two_sided(falses[i].call, falses[i].actual, falses[i].expected));
        mt_drop(falses[i].call), mt_drop(falses[i].actual), mt_drop(falses[i].expected);
    }
    check_answers("a true containment", mt_eval(m, E("assert-includes-answers", B(true), mt_keep(check2), E(1, 2, 3), E(1, 2))), B(true));
    mt_atom *actual = E(1, 2), *expected = E(7);
    mt_atom *ball = reported(m, E("assert-includes-answers", B(false), mt_keep(check2), mt_keep(actual), mt_keep(expected)));
    require("the door raised", ball != NULL);
    check_atom("a one-sided report", ball, one_sided(check2, actual, expected));

    mt_atom *equal = E("assertEqual", E("+", 1, 1), 3), *sum = E(1 + 1), *three = E(3);
    ball = reported(m, mt_keep(equal));
    require("assertEqual failed", ball != NULL);
    check_atom("assertEqual reports through the two-sided door", ball, two_sided(equal, sum, three));
    mt_atom *includes = E("assertIncludes", E("superpose", mt_keep(actual)), mt_keep(expected));
    ball = reported(m, mt_keep(includes));
    require("assertIncludes failed", ball != NULL);
    check_atom("assertIncludes through the one-sided one", ball, one_sided(includes, actual, expected));

    require("(: assert-sorted (-> Atom Bool))", mt_add(m, E(":", "assert-sorted", E("->", "Atom", "Bool"))));
    require("define assert-sorted", mt_add(m, E("=", E("assert-sorted", V("expression")),
        E("let*", E(E(V("answers"), E("collapse", V("expression"))), E(V("sorted"), E("sort-atom", V("answers")))),
          E("assert-answers", E("==", V("answers"), V("sorted")), E("assert-sorted", V("expression")), V("answers"), V("sorted"))))));
    mt_atom *in_order = E(1, 2, 3), *in_order_sorted = sorted_bag(in_order);
    check_answers("a sorted bag passes", mt_eval(m, E("assert-sorted", E("superpose", mt_keep(in_order)))), B(mt_eq(in_order, in_order_sorted)));
    mt_atom *shuffled = E(3, 1, 2), *call = E("assert-sorted", E("superpose", mt_keep(shuffled))), *shuffled_sorted = sorted_bag(shuffled);
    ball = reported(m, mt_keep(call));
    require("assert-sorted failed", ball != NULL);
    check_atom("an unsorted one fails with its bag intact", ball, two_sided(call, shuffled, shuffled_sorted));
    mt_atom *held[] = { check1, check2, actual, expected, equal, sum, three, includes, in_order, in_order_sorted, shuffled, call, shuffled_sorted };
    for (size_t i = 0; i < sizeof held / sizeof *held; i++) mt_drop(held[i]);
    return done(m);
}
