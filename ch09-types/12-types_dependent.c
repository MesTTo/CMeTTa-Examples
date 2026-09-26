/* Purpose: a type that depends on a value. get-type is extended by
 *   equations: a number whose remainder by 2 is 0 is an EvenNumber, and a
 *   list whose every element is one is an EvenNumberList. C decides the same
 *   by its own %, so f admits two even numbers and answers their sum, and g
 *   admits an all-even list.
 * Guarantees: both claims of the original hold
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

static bool even(int64_t x) { return x % 2 == 0; }

static bool all_even(const int64_t *xs, size_t n)
{
    for (size_t i = 0; i < n; i++)
        if (!even(xs[i])) return false;
    return true;
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("(= (get-type $x) ...)", mt_add(m, E("=", E("get-type", V("x")),
        E("catch", E("let", V("remainder"), E("%", V("x"), 2), E("if", E("=alpha", V("remainder"), 0), "EvenNumber"))))));
    require("(: f (-> EvenNumber EvenNumber EvenNumber))", mt_add(m, E(":", "f", E("->", "EvenNumber", "EvenNumber", "EvenNumber"))));
    require("(= (f $x $y) (+ $x $y))", mt_add(m, E("=", E("f", V("x"), V("y")), E("+", V("x"), V("y")))));
    const int64_t pair[] = { 2, 4 };
    require("both even", all_even(pair, 2));
    assert(answers_are(mt_eval(m, E("f", pair[0], pair[1])), E(N(pair[0] + pair[1]))) && "two EvenNumbers are admitted");

    require("(= (get-type (cons $head $tail)) ...)", mt_add(m, E("=", E("get-type", E("cons", V("head"), V("tail"))),
        E("let", V("head-type"), E("get-type", V("head")),
          E("if", E("=alpha", V("head-type"), "EvenNumber"), E("if", E("=alpha", V("tail"), mt_unit()), "EvenNumberList", E("get-type", V("tail"))))))));
    require("(: g (-> EvenNumberList Bool))", mt_add(m, E(":", "g", E("->", "EvenNumberList", "Bool"))));
    require("(= (g $L) True)", mt_add(m, E("=", E("g", V("L")), B(true))));
    const int64_t list[] = { 2, 4, 6 };
    assert(answers_are(mt_eval(m, E("g", E(list[0], list[1], list[2]))), E(B(all_even(list, 3)))) && "an all-even list is admitted");
    mt_close(m);
    return 0;
}
