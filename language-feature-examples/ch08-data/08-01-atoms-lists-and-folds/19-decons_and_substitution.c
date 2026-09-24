/* Purpose: one step apart, one value in. decons-atom answers head and tail
 *   together, which C builds from mt_at(e, 0) and a view over the rest, and
 *   cons puts them back. atom-subst puts a value where a named variable
 *   stands, which in C is mt_unify of the variable with the value, a
 *   substitution of one binding, applied with mt_substitute; the binder
 *   position is held, so a call there is refused.
 * Guarantees: all thirteen claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

static void drop_parent(void *parent) { mt_drop(parent); }

static mt_atom *split(const mt_atom *e)
{
    return E(mt_keep(mt_at(e, 0)), mt_expr_ref(mt_len(e) - 1, mt_children(e) + 1, mt_keep(e), drop_parent));
}

/* `value` put where `var` stands in `term`, by C. */
static mt_atom *substituted(mt_atom *value, mt_atom *var, mt_atom *term)
{
    mt_bindings *theta = mt_unify(var, value);
    mt_atom *out = theta ? mt_substitute(term, theta) : NULL;
    mt_bindings_free(theta);
    mt_drop(value);
    mt_drop(var);
    mt_drop(term);
    return out;
}

int main(void)
{
    metta *m = open_engine();
    mt_atom *abc = E("a", "b", "c"), *a = E("a"), *nums = E(1, 2, 3), *call_first = E(E("f", 1), "b");

    check_answers("decons-atom", mt_eval(m, E("decons-atom", mt_keep(abc))), split(abc));
    check_answers("of one element", mt_eval(m, E("decons-atom", mt_keep(a))), split(a));
    mt_atom *parts = split(nums);
    check_answers("destructured by let", mt_eval(m, E("let", E(V("head"), V("tail")), E("decons-atom", mt_keep(nums)),
                                                    E("+", V("head"), E("car-atom", V("tail"))))),
                  mt_int(mt_at(parts, 0)) + mt_int(mt_at(mt_at(parts, 1), 0)));
    mt_drop(parts);
    check_answers("decons is the same operation", mt_eval(m, E("decons", mt_keep(abc))), split(abc));
    check_answers("under either name", mt_eval(m, E("==", E("decons", mt_keep(abc)), E("decons-atom", mt_keep(abc)))), B(true));
    check_answers("cons puts back what decons took",
                  mt_eval(m, E("let", E(V("head"), V("tail")), E("decons", mt_keep(abc)), E("cons", V("head"), V("tail")))),
                  mt_keep(abc));
    check_answers("a call in head position stays a call", mt_eval(m, E("decons", mt_keep(call_first))), split(call_first));

    check_answers("every occurrence", mt_eval(m, E("atom-subst", 1, V("x"), E("foo", V("x"), V("x")))),
                  substituted(N(1), V("x"), E("foo", V("x"), V("x"))));
    check_answers("a value that is a term", mt_eval(m, E("atom-subst", E("g", 2), V("x"), E("foo", V("x"), E("bar", V("x"))))),
                  substituted(E("g", 2), V("x"), E("foo", V("x"), E("bar", V("x")))));
    check_answers("only the variable named", mt_eval(m, E("atom-subst", 1, V("y"), E("foo", V("x"), V("y")))),
                  substituted(N(1), V("y"), E("foo", V("x"), V("y"))));
    check_answers("a call in the binder position is refused",
                  mt_eval(m, E("atom-subst", 1, E("car-atom", E(V("x"))), E("foo", V("x")))),
                  E("Error", E("atom-subst", 1, E("car-atom", E(V("x"))), E("foo", V("x"))), "NoReturn"));
    check_answers("nothing to substitute", mt_eval(m, E("atom-subst", 1, V("x"), E("foo", "bar"))),
                  substituted(N(1), V("x"), E("foo", "bar")));
    mt_drop(abc);
    mt_drop(a);
    mt_drop(nums);
    mt_drop(call_first);
    return done(m);
}
