/* Purpose: held deconstruction and its fallback. if-decons-expr splits an
 *   expression into head and tail, or takes the fallback when there is
 *   nothing to split. C splits the same expression itself, the head
 *   mt_at(e, 0) and the tail a view over the rest, and decides the bound
 *   binders' cases with mt_unify; the engine's answers must match C's.
 * Guarantees: all ten claims of the original hold
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

/* (head tail) of a non-empty expression, or NULL. */
static mt_atom *split(const mt_atom *e)
{
    if (mt_kind_of(e) != MT_EXPR || mt_len(e) == 0) return NULL;
    return E(mt_keep(mt_at(e, 0)), mt_expr_ref(mt_len(e) - 1, mt_children(e) + 1, mt_keep(e), drop_parent));
}

/* What C says (pair head tail), or fallback, is for `e`. */
static mt_atom *paired(const mt_atom *e)
{
    mt_atom *parts = split(e);
    mt_atom *out = parts ? E("pair", mt_keep(mt_at(parts, 0)), mt_keep(mt_at(parts, 1))) : S("fallback");
    mt_drop(parts);
    return out;
}

/* matched when the binders unify with the split, else fallback. */
static mt_atom *bound(const mt_atom *e, mt_atom *head, mt_atom *tail)
{
    mt_atom *parts = split(e), *binders = E(head, tail);
    mt_bindings *theta = parts ? mt_unify(binders, parts) : NULL;
    mt_atom *out = S(theta ? "matched" : "fallback");
    mt_bindings_free(theta);
    mt_drop(parts);
    mt_drop(binders);
    return out;
}

static mt_atom *decons(mt_atom *e, mt_atom *head, mt_atom *tail, mt_atom *then, mt_atom *otherwise)
{
    return E("if-decons-expr", e, head, tail, then, otherwise);
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    mt_atom *abc = E("a", "b", "c"), *none = mt_unit(), *unknown = V("unknown"), *ab = E("a", "b");
    mt_atom *held = E("+", 1, 2);

    assert(answers_are(mt_eval(m, decons(mt_keep(abc), V("h"), V("t"), E("pair", V("h"), V("t")), S("fallback"))), E(paired(abc)))
           && "a list splits");
    assert(answers_are(mt_eval(m, decons(mt_keep(none), V("h"), V("t"), E("pair", V("h"), V("t")), S("fallback"))), E(paired(none)))
           && "the empty list falls back");
    assert(answers_are(mt_eval(m, decons(mt_keep(unknown), V("h"), V("t"), E("pair", V("h"), V("t")), S("fallback"))), E(paired(unknown)))
           && "so does a variable");
    mt_atom *parts = split(held);
    assert(answers_are(mt_eval(m, decons(mt_keep(held), V("h"), V("t"), V("t"), S("fallback"))), E(mt_keep(mt_at(parts, 1))))
           && "the call is split as written");
    mt_drop(parts);
    assert(answers_are(mt_eval(m, decons(E(1, 2), V("h"), V("t"), E("+", V("h"), 10), E("/", 1, 0))), E(11)) && "the branch taken is evaluated");
    assert(answers_are(mt_eval(m, decons(mt_keep(ab), S("a"), E("b"), S("matched"), S("fallback"))), E(bound(ab, S("a"), E("b"))))
           && "bound binders that fit");
    assert(answers_are(mt_eval(m, decons(mt_keep(ab), S("wrong"), E("b"), S("matched"), S("fallback"))), E(bound(ab, S("wrong"), E("b"))))
           && "a head that does not");
    assert(answers_are(mt_eval(m, decons(mt_keep(ab), S("a"), mt_unit(), S("matched"), S("fallback"))), E(bound(ab, S("a"), mt_unit())))
           && "a tail that does not");
    assert(answers_are(mt_eval(m, decons(mt_keep(ab), V("h"), V("t"), E("superpose", E(1, 2, 2)), S("fallback"))), E(1, 2, 2))
           && "the branch may fork");
    assert(answers_are(mt_eval(m, E("if-error", decons(N(42), V("h"), V("t"), S("yes"), S("fallback")),
                                     "refused", "answered")), E("refused"))
           && "a number is refused");
    mt_drop(abc);
    mt_drop(none);
    mt_drop(unknown);
    mt_drop(ab);
    mt_drop(held);
    mt_close(m);
    return 0;
}
