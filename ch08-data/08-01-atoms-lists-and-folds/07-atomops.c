/* Purpose: structure, and what refuses. An expression is an array of
 *   children, so cons-atom, car-atom, cdr-atom and index-atom are C's
 *   prepend, first element, tail view and index, and =alpha is mt_alpha_eq;
 *   each engine answer is held against C's. An operand an operation cannot
 *   use answers the empty expression; an unbound variable where an
 *   expression belongs is refused, while a bound one is answered, and
 *   index-atom still enumerates over an unbound index.
 * Guarantees: all thirty claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>

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

static void drop_parent(void *parent) { mt_drop(parent); }

/* (if-error (catch call) refused answered) */
static mt_atom *verdict(mt_atom *call) { return E("if-error", E("catch", call), "refused", "answered"); }

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    mt_atom *e = E(1, 2, 3);

    mt_atom *prepended[4] = { N(0), mt_keep(mt_at(e, 0)), mt_keep(mt_at(e, 1)), mt_keep(mt_at(e, 2)) };
    assert(answers_are(mt_eval(m, E("cons-atom", 0, mt_keep(e))), E(mt_exprv(4, prepended))) && "cons-atom prepends");
    assert(answers_are(mt_eval(m, E("car-atom", mt_keep(e))), E(mt_keep(mt_at(e, 0)))) && "car-atom is the first child");
    assert(answers_are(mt_eval(m, E("cdr-atom", mt_keep(e))), E(mt_expr_ref(mt_len(e) - 1, mt_children(e) + 1, mt_keep(e), drop_parent)))
           && "cdr-atom is the rest");
    assert(answers_are(mt_eval(m, E("index-atom", mt_keep(e), 1)), E(mt_keep(mt_at(e, 1)))) && "index-atom indexes");
    assert(answers_are(mt_eval(m, E("id", 5)), E(5)) && "(id 5)");

    mt_atom *fx = E("Father", V("X")), *fy = E("Father", V("Y")), *sx = E("Son", V("X"));
    assert(answers_are(mt_eval(m, E("=alpha", mt_keep(fx), mt_keep(fy))), E(B(mt_alpha_eq(fx, fy)))) && "=alpha renames");
    assert(answers_are(mt_eval(m, E("=alpha", mt_keep(fx), mt_keep(sx))), E(B(mt_alpha_eq(fx, sx)))) && "but a functor is a functor");
    mt_drop(fx);
    mt_drop(fy);
    mt_drop(sx);
    assert(answers_are(mt_eval(m, E("first-from-pair", E("A", "B"))), E("A")) && "first-from-pair");
    assert(answers_are(mt_eval(m, E("second-from-pair", E("A", "B"))), E("B")) && "second-from-pair");

    assert(answers_are(mt_eval(m, E("index-atom", mt_keep(e), 5)), E(mt_unit())) && "past the end");
    assert(answers_are(mt_eval(m, E("index-atom", mt_keep(e), "a")), E(mt_unit())) && "not an index");
    const char *const on_a_number[] = { "size-atom", "sort-atom", "unique-atom", "alpha-unique-atom",
                                        "min-atom", "max-atom" };
    for (size_t i = 0; i < sizeof on_a_number / sizeof *on_a_number; i++)
        assert(answers_are(mt_eval(m, E(on_a_number[i], 5)), E(mt_unit())) && on_a_number[i]);
    assert(answers_are(mt_eval(m, E("intersection-atom", 5, E("a"))), E(mt_unit())) && "intersection-atom of a number");

    const char *const guarded[] = { "car-atom", "size-atom", "sort-atom" };
    for (size_t i = 0; i < sizeof guarded / sizeof *guarded; i++)
        assert(answers_are(mt_eval(m, verdict(E(guarded[i], V("unbound")))), E("refused")) && guarded[i]);
    assert(answers_are(mt_eval(m, verdict(E("index-atom", V("unbound"), 0))), E("refused")) && "index-atom's list");
    assert(answers_are(mt_eval(m, verdict(E("subtraction-atom", V("unbound"), E("a", "b")))), E("refused")) && "subtraction-atom's");

    assert(answers_are(mt_eval(m, verdict(E("car-atom", E(1, 2)))), E("answered")) && "a bound argument is answered");
    assert(answers_are(mt_eval(m, E("car-atom", E(1, 2))), E(1)) && "(car-atom (1 2))");
    assert(answers_are(mt_eval(m, E("index-atom", E("a", "b", "c"), V("i"))), E("a", "b", "c")) && "an unbound index enumerates");
    mt_drop(e);
    mt_close(m);
    return 0;
}
