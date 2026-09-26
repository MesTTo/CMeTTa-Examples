/* Purpose: one step apart, one value in. decons-atom answers head and tail
 *   together, which C builds from mt_at(e, 0) and a view over the rest, and
 *   cons puts them back. atom-subst puts a value where a named variable
 *   stands, which in C is mt_unify of the variable with the value, a
 *   substitution of one binding, applied with mt_substitute; the binder
 *   position is held, so a call there is refused.
 * Guarantees: all thirteen claims of the original hold
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
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    mt_atom *abc = E("a", "b", "c"), *a = E("a"), *nums = E(1, 2, 3), *call_first = E(E("f", 1), "b");

    assert(answers_are(mt_eval(m, E("decons-atom", mt_keep(abc))), E(split(abc))) && "decons-atom");
    assert(answers_are(mt_eval(m, E("decons-atom", mt_keep(a))), E(split(a))) && "of one element");
    mt_atom *parts = split(nums);
    assert(answers_are(mt_eval(m, E("let", E(V("head"), V("tail")), E("decons-atom", mt_keep(nums)),
                                  E("+", V("head"), E("car-atom", V("tail"))))), E(mt_int(mt_at(parts, 0)) + mt_int(mt_at(mt_at(parts, 1), 0))))
           && "destructured by let");
    mt_drop(parts);
    assert(answers_are(mt_eval(m, E("decons", mt_keep(abc))), E(split(abc))) && "decons is the same operation");
    assert(answers_are(mt_eval(m, E("==", E("decons", mt_keep(abc)), E("decons-atom", mt_keep(abc)))), E(B(true))) && "under either name");
    assert(answers_are(mt_eval(m, E("let", E(V("head"), V("tail")), E("decons", mt_keep(abc)), E("cons", V("head"), V("tail")))), E(mt_keep(abc)))
           && "cons puts back what decons took");
    assert(answers_are(mt_eval(m, E("decons", mt_keep(call_first))), E(split(call_first))) && "a call in head position stays a call");

    assert(answers_are(mt_eval(m, E("atom-subst", 1, V("x"), E("foo", V("x"), V("x")))), E(substituted(N(1), V("x"), E("foo", V("x"), V("x")))))
           && "every occurrence");
    assert(answers_are(mt_eval(m, E("atom-subst", E("g", 2), V("x"), E("foo", V("x"), E("bar", V("x"))))), E(substituted(E("g", 2), V("x"), E("foo", V("x"), E("bar", V("x"))))))
           && "a value that is a term");
    assert(answers_are(mt_eval(m, E("atom-subst", 1, V("y"), E("foo", V("x"), V("y")))), E(substituted(N(1), V("y"), E("foo", V("x"), V("y")))))
           && "only the variable named");
    assert(answers_are(mt_eval(m, E("atom-subst", 1, E("car-atom", E(V("x"))), E("foo", V("x")))), E(E("Error", E("atom-subst", 1, E("car-atom", E(V("x"))), E("foo", V("x"))), "NoReturn")))
           && "a call in the binder position is refused");
    assert(answers_are(mt_eval(m, E("atom-subst", 1, V("x"), E("foo", "bar"))), E(substituted(N(1), V("x"), E("foo", "bar"))))
           && "nothing to substitute");
    mt_drop(abc);
    mt_drop(a);
    mt_drop(nums);
    mt_drop(call_first);
    mt_close(m);
    return 0;
}
