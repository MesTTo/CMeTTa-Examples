/* Purpose: the two-sided fragments, reached through unify, whose operands
 *   are both syntax. One side with no gap is still one-sided; in the last
 *   position fragment a trailing gap takes the other side's remainder, which
 *   C takes as a slice, open gaps included; a name may repeat there; the
 *   fragment is unitary; linear shallow gaps trade the settled children
 *   between them; a space operand makes the ask a query, answering what
 *   match answers; and outside every fragment the ask refuses.
 * Guarantees: all fifteen claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"
#include "segments.h"

static mt_atom *unify(mt_atom *a, mt_atom *b, mt_atom *then, mt_atom *otherwise)
{
    return E("unify", a, b, then, otherwise);
}

int main(void)
{
    metta *m = open_engine();
    mt_atom *fab = E("f", "a", "b"), *longer = E("f", "a", "b", seg("v")), *whole = E("f", seg("v"));

    check_answers("a gap on the right alone", mt_eval(m, unify(E("f", "a", "b"), E("f", "a", seg("v")), V("v"), S("none"))),
                  run(fab, 2, 3));
    check_answers("a trailing gap takes an empty remainder",
                  mt_eval(m, unify(mt_keep(fab), E("f", "a", "b", seg("v")), V("v"), S("none"))), mt_unit());
    check_answers("a remainder keeps the other side's gap",
                  mt_eval(m, unify(E("f", "a", seg("u")), mt_keep(longer), V("u"), S("none"))), run(longer, 2, 4));
    check_answers("as the marker that would match it",
                  mt_eval(m, unify(E("f", seg("u")), mt_keep(whole), V("u"), S("none"))), run(whole, 1, 2));
    check_answers("an anonymous gap absorbs one", mt_eval(m, unify(E("f", GAP()), E("f", seg("v")), S("taken"), S("none"))), "taken");
    check_answers("a name may repeat here",
                  mt_eval(m, unify(E("f", E("g", seg("x")), E("h", seg("x"))), E("f", E("g", seg("y")), E("h", "b")), V("x"), S("none"))),
                  E("b"));
    check_answers("X = X holds", mt_eval(m, unify(E("f", seg("u")), E("f", seg("u")), S("yes"), S("no"))), "yes");
    check_list("the fragment is unitary", mt_all(mt_eval(m, unify(mt_keep(fab), E("f", "a", "b", seg("v")), V("v"), S("none")))),
               mt_unit());

    check_answers("linear shallow gaps trade children",
                  mt_eval(m, unify(E("f", seg("u"), "b"), E("f", "a", seg("v")), E(V("u"), V("v")), S("no"))), E(E("a"), E("b")));
    check_answers("expressions included",
                  mt_eval(m, unify(E("f", seg("u"), E("g", "a")), E("f", E("g", "b"), seg("v")), E(V("u"), V("v")), S("no"))),
                  E(E(E("g", "b")), E(E("g", "a"))));
    check_list("once each", mt_all(mt_eval(m, unify(E("f", seg("u"), "b"), E("f", "a", seg("v")), E(V("u"), V("v")), S("no")))),
               E(E("a"), E("b")));

    mt_atom *bob = E("friend", "Bob", "Alice"), *carol = E("friend", "Carol", "Alice");
    require("(friend Bob Alice)", mt_add(m, mt_keep(bob)));
    require("(friend Carol Alice)", mt_add(m, mt_keep(carol)));
    check_answers("a space operand is a query", mt_eval(m, unify(mt_spaceref("&self"), E("friend", seg("who")), V("who"), S("none"))),
                  run(bob, 1, 3), run(carol, 1, 3));
    check_answers("answering what match answers", mt_eval(m, E("match", "&self", E("friend", seg("who")), V("who"))),
                  run(bob, 1, 3), run(carol, 1, 3));
    check_answers("outside every fragment it refuses",
                  mt_eval(m, E("if-error", E("catch", unify(E("f", seg("x"), "a"), E("f", "a", seg("x")), S("yes"), S("no"))),
                               "refused", "answered")), "refused");
    mt_atom *all[] = { fab, longer, whole, bob, carol };
    for (size_t i = 0; i < sizeof all / sizeof *all; i++) mt_drop(all[i]);
    return done(m);
}
