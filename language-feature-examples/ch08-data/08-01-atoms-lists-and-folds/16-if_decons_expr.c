/* Purpose: held deconstruction and its fallback. if-decons-expr splits an
 *   expression into head and tail, or takes the fallback when there is
 *   nothing to split. C splits the same expression itself, the head
 *   mt_at(e, 0) and the tail a view over the rest, and decides the bound
 *   binders' cases with mt_unify; the engine's answers must match C's.
 * Guarantees: all ten claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

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
    metta *m = open_engine();
    mt_atom *abc = E("a", "b", "c"), *none = mt_unit(), *unknown = V("unknown"), *ab = E("a", "b");
    mt_atom *held = E("+", 1, 2);

    check_answers("a list splits", mt_eval(m, decons(mt_keep(abc), V("h"), V("t"), E("pair", V("h"), V("t")), S("fallback"))),
                  paired(abc));
    check_answers("the empty list falls back", mt_eval(m, decons(mt_keep(none), V("h"), V("t"), E("pair", V("h"), V("t")), S("fallback"))),
                  paired(none));
    check_answers("so does a variable", mt_eval(m, decons(mt_keep(unknown), V("h"), V("t"), E("pair", V("h"), V("t")), S("fallback"))),
                  paired(unknown));
    mt_atom *parts = split(held);
    check_answers("the call is split as written", mt_eval(m, decons(mt_keep(held), V("h"), V("t"), V("t"), S("fallback"))),
                  mt_keep(mt_at(parts, 1)));
    mt_drop(parts);
    check_answers("the branch taken is evaluated", mt_eval(m, decons(E(1, 2), V("h"), V("t"), E("+", V("h"), 10), E("/", 1, 0))), 11);
    check_answers("bound binders that fit", mt_eval(m, decons(mt_keep(ab), S("a"), E("b"), S("matched"), S("fallback"))),
                  bound(ab, S("a"), E("b")));
    check_answers("a head that does not", mt_eval(m, decons(mt_keep(ab), S("wrong"), E("b"), S("matched"), S("fallback"))),
                  bound(ab, S("wrong"), E("b")));
    check_answers("a tail that does not", mt_eval(m, decons(mt_keep(ab), S("a"), mt_unit(), S("matched"), S("fallback"))),
                  bound(ab, S("a"), mt_unit()));
    check_answers("the branch may fork", mt_eval(m, decons(mt_keep(ab), V("h"), V("t"), E("superpose", E(1, 2, 2)), S("fallback"))),
                  1, 2, 2);
    check_answers("a number is refused", mt_eval(m, E("if-error", decons(N(42), V("h"), V("t"), S("yes"), S("fallback")),
                                                       "refused", "answered")), "refused");
    mt_drop(abc);
    mt_drop(none);
    mt_drop(unknown);
    mt_drop(ab);
    mt_drop(held);
    return done(m);
}
