/* Purpose: one answer, from an equation that can be edited. eval-one takes
 *   the first answer of its argument; unique-value has two equations, one of
 *   which answers nothing, so the answer is 11; noeval holds a term back;
 *   and replacing the equation that answered 11 changes the next answer.
 * Guarantees: all five claims of the original hold
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

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("(= (unique-value) 11)", mt_add(m, E("=", E("unique-value"), 11)));
    require("(= (unique-value) (superpose ()))", mt_add(m, E("=", E("unique-value"), E("superpose", mt_unit()))));

    assert(mt_one_int(mt_eval(m, E("eval-one", E("+", 1, 2)))) == 3 && "eval-one of (+ 1 2)");
    assert(mt_one_int(mt_eval(m, E("eval-one", E("unique-value")))) == 11
           && "a failing alternative adds no answer");
    assert(answers_are(mt_eval(m, E("eval-one", E("noeval", E("+", 2, 3)))), E(E("+", 2, 3)))
           && "noeval holds the term back unreduced");
    assert(answers_are(mt_eval(m, E("eval-one", E("noeval", mt_unit()))), E(mt_unit())) && "and the empty expression");

    require("remove the equation that answered 11", mt_del(m, E("=", E("unique-value"), 11)));
    require("(= (unique-value) 22)", mt_add(m, E("=", E("unique-value"), 22)));
    assert(mt_one_int(mt_eval(m, E("eval-one", E("unique-value")))) == 22 && "the edit changes the next answer");
    mt_close(m);
    return 0;
}
