/* Purpose: a type that depends on a value. get-type is extended by
 *   equations: a number whose remainder by 2 is 0 is an EvenNumber, and a
 *   list whose every element is one is an EvenNumberList. C decides the same
 *   by its own %, so f admits two even numbers and answers their sum, and g
 *   admits an all-even list.
 * Guarantees: both claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

static bool even(int64_t x) { return x % 2 == 0; }

static bool all_even(const int64_t *xs, size_t n)
{
    for (size_t i = 0; i < n; i++)
        if (!even(xs[i])) return false;
    return true;
}

int main(void)
{
    metta *m = open_engine();
    require("(= (get-type $x) ...)", mt_add(m, E("=", E("get-type", V("x")),
        E("catch", E("let", V("remainder"), E("%", V("x"), 2), E("if", E("=alpha", V("remainder"), 0), "EvenNumber"))))));
    require("(: f (-> EvenNumber EvenNumber EvenNumber))", mt_add(m, E(":", "f", E("->", "EvenNumber", "EvenNumber", "EvenNumber"))));
    require("(= (f $x $y) (+ $x $y))", mt_add(m, E("=", E("f", V("x"), V("y")), E("+", V("x"), V("y")))));
    const int64_t pair[] = { 2, 4 };
    require("both even", all_even(pair, 2));
    check_answers("two EvenNumbers are admitted", mt_eval(m, E("f", pair[0], pair[1])), N(pair[0] + pair[1]));

    require("(= (get-type (cons $head $tail)) ...)", mt_add(m, E("=", E("get-type", E("cons", V("head"), V("tail"))),
        E("let", V("head-type"), E("get-type", V("head")),
          E("if", E("=alpha", V("head-type"), "EvenNumber"), E("if", E("=alpha", V("tail"), mt_unit()), "EvenNumberList", E("get-type", V("tail"))))))));
    require("(: g (-> EvenNumberList Bool))", mt_add(m, E(":", "g", E("->", "EvenNumberList", "Bool"))));
    require("(= (g $L) True)", mt_add(m, E("=", E("g", V("L")), B(true))));
    const int64_t list[] = { 2, 4, 6 };
    check_answers("an all-even list is admitted", mt_eval(m, E("g", E(list[0], list[1], list[2]))), B(all_even(list, 3)));
    return done(m);
}
