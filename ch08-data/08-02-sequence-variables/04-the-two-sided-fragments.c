/* Purpose: the two-sided fragments, reached through unify, whose operands
 *   are both syntax. One side with no gap is still one-sided; in the last
 *   position fragment a trailing gap takes the other side's remainder, which
 *   C takes as a slice, open gaps included; a name may repeat there; the
 *   fragment is unitary; linear shallow gaps trade the settled children
 *   between them; a space operand makes the ask a query, answering what
 *   match answers; and outside every fragment the ask refuses.
 * Guarantees: all fifteen claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include "_fixtures/segments.h"

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

static mt_atom *unify(mt_atom *a, mt_atom *b, mt_atom *then, mt_atom *otherwise)
{
    return E("unify", a, b, then, otherwise);
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    mt_atom *fab = E("f", "a", "b"), *longer = E("f", "a", "b", seg("v")), *whole = E("f", seg("v"));

    assert(answers_are(mt_eval(m, unify(E("f", "a", "b"), E("f", "a", seg("v")), V("v"), S("none"))), E(run(fab, 2, 3)))
           && "a gap on the right alone");
    assert(answers_are(mt_eval(m, unify(mt_keep(fab), E("f", "a", "b", seg("v")), V("v"), S("none"))), E(mt_unit()))
           && "a trailing gap takes an empty remainder");
    assert(answers_are(mt_eval(m, unify(E("f", "a", seg("u")), mt_keep(longer), V("u"), S("none"))), E(run(longer, 2, 4)))
           && "a remainder keeps the other side's gap");
    assert(answers_are(mt_eval(m, unify(E("f", seg("u")), mt_keep(whole), V("u"), S("none"))), E(run(whole, 1, 2)))
           && "as the marker that would match it");
    assert(answers_are(mt_eval(m, unify(E("f", GAP()), E("f", seg("v")), S("taken"), S("none"))), E("taken")) && "an anonymous gap absorbs one");
    assert(answers_are(mt_eval(m, unify(E("f", E("g", seg("x")), E("h", seg("x"))), E("f", E("g", seg("y")), E("h", "b")), V("x"), S("none"))), E(E("b")))
           && "a name may repeat here");
    assert(answers_are(mt_eval(m, unify(E("f", seg("u")), E("f", seg("u")), S("yes"), S("no"))), E("yes")) && "X = X holds");
    assert(list_is(mt_all(mt_eval(m, unify(mt_keep(fab), E("f", "a", "b", seg("v")), V("v"), S("none")))), E(mt_unit()))
           && "the fragment is unitary");

    assert(answers_are(mt_eval(m, unify(E("f", seg("u"), "b"), E("f", "a", seg("v")), E(V("u"), V("v")), S("no"))), E(E(E("a"), E("b"))))
           && "linear shallow gaps trade children");
    assert(answers_are(mt_eval(m, unify(E("f", seg("u"), E("g", "a")), E("f", E("g", "b"), seg("v")), E(V("u"), V("v")), S("no"))), E(E(E(E("g", "b")), E(E("g", "a")))))
           && "expressions included");
    assert(list_is(mt_all(mt_eval(m, unify(E("f", seg("u"), "b"), E("f", "a", seg("v")), E(V("u"), V("v")), S("no")))), E(E(E("a"), E("b"))))
           && "once each");

    mt_atom *bob = E("friend", "Bob", "Alice"), *carol = E("friend", "Carol", "Alice");
    require("(friend Bob Alice)", mt_add(m, mt_keep(bob)));
    require("(friend Carol Alice)", mt_add(m, mt_keep(carol)));
    assert(answers_are(mt_eval(m, unify(mt_spaceref("&self"), E("friend", seg("who")), V("who"), S("none"))), E(run(bob, 1, 3), run(carol, 1, 3)))
           && "a space operand is a query");
    assert(answers_are(mt_eval(m, E("match", "&self", E("friend", seg("who")), V("who"))), E(run(bob, 1, 3), run(carol, 1, 3)))
           && "answering what match answers");
    assert(answers_are(mt_eval(m, E("if-error", E("catch", unify(E("f", seg("x"), "a"), E("f", "a", seg("x")), S("yes"), S("no"))),
                                    "refused", "answered")), E("refused"))
           && "outside every fragment it refuses");
    mt_atom *all[] = { fab, longer, whole, bob, carol };
    for (size_t i = 0; i < sizeof all / sizeof *all; i++) mt_drop(all[i]);
    mt_close(m);
    return 0;
}
