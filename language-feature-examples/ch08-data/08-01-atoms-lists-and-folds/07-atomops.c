/* Purpose: structure, and what refuses. An expression is an array of
 *   children, so cons-atom, car-atom, cdr-atom and index-atom are C's
 *   prepend, first element, tail view and index, and =alpha is mt_alpha_eq;
 *   each engine answer is held against C's. An operand an operation cannot
 *   use answers the empty expression; an unbound variable where an
 *   expression belongs is refused, while a bound one is answered, and
 *   index-atom still enumerates over an unbound index.
 * Guarantees: all thirty claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

static void drop_parent(void *parent) { mt_drop(parent); }

/* (if-error (catch call) refused answered) */
static mt_atom *verdict(mt_atom *call) { return E("if-error", E("catch", call), "refused", "answered"); }

int main(void)
{
    metta *m = open_engine();
    mt_atom *e = E(1, 2, 3);

    mt_atom *prepended[4] = { N(0), mt_keep(mt_at(e, 0)), mt_keep(mt_at(e, 1)), mt_keep(mt_at(e, 2)) };
    check_answers("cons-atom prepends", mt_eval(m, E("cons-atom", 0, mt_keep(e))), mt_exprv(4, prepended));
    check_answers("car-atom is the first child", mt_eval(m, E("car-atom", mt_keep(e))), mt_keep(mt_at(e, 0)));
    check_answers("cdr-atom is the rest", mt_eval(m, E("cdr-atom", mt_keep(e))),
                  mt_expr_ref(mt_len(e) - 1, mt_children(e) + 1, mt_keep(e), drop_parent));
    check_answers("index-atom indexes", mt_eval(m, E("index-atom", mt_keep(e), 1)), mt_keep(mt_at(e, 1)));
    check_answers("(id 5)", mt_eval(m, E("id", 5)), 5);

    mt_atom *fx = E("Father", V("X")), *fy = E("Father", V("Y")), *sx = E("Son", V("X"));
    check_answers("=alpha renames", mt_eval(m, E("=alpha", mt_keep(fx), mt_keep(fy))), B(mt_alpha_eq(fx, fy)));
    check_answers("but a functor is a functor", mt_eval(m, E("=alpha", mt_keep(fx), mt_keep(sx))), B(mt_alpha_eq(fx, sx)));
    mt_drop(fx);
    mt_drop(fy);
    mt_drop(sx);
    check_answers("first-from-pair", mt_eval(m, E("first-from-pair", E("A", "B"))), "A");
    check_answers("second-from-pair", mt_eval(m, E("second-from-pair", E("A", "B"))), "B");

    check_answers("past the end", mt_eval(m, E("index-atom", mt_keep(e), 5)), mt_unit());
    check_answers("not an index", mt_eval(m, E("index-atom", mt_keep(e), "a")), mt_unit());
    const char *const on_a_number[] = { "size-atom", "sort-atom", "unique-atom", "alpha-unique-atom",
                                        "min-atom", "max-atom" };
    for (size_t i = 0; i < sizeof on_a_number / sizeof *on_a_number; i++)
        check_answers(on_a_number[i], mt_eval(m, E(on_a_number[i], 5)), mt_unit());
    check_answers("intersection-atom of a number", mt_eval(m, E("intersection-atom", 5, E("a"))), mt_unit());

    const char *const guarded[] = { "car-atom", "size-atom", "sort-atom" };
    for (size_t i = 0; i < sizeof guarded / sizeof *guarded; i++)
        check_answers(guarded[i], mt_eval(m, verdict(E(guarded[i], V("unbound")))), "refused");
    check_answers("index-atom's list", mt_eval(m, verdict(E("index-atom", V("unbound"), 0))), "refused");
    check_answers("subtraction-atom's", mt_eval(m, verdict(E("subtraction-atom", V("unbound"), E("a", "b")))), "refused");

    check_answers("a bound argument is answered", mt_eval(m, verdict(E("car-atom", E(1, 2)))), "answered");
    check_answers("(car-atom (1 2))", mt_eval(m, E("car-atom", E(1, 2))), 1);
    check_answers("an unbound index enumerates", mt_eval(m, E("index-atom", E("a", "b", "c"), V("i"))), "a", "b", "c");
    mt_drop(e);
    return done(m);
}
