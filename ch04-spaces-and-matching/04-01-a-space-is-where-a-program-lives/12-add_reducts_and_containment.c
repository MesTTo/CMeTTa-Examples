/* Purpose: answers in, or programs in. mt_add_all() stores atoms as
 *   written, so &written holds calls; add-reducts evaluates each first, so
 *   &reduced holds answers; and space-contains asks one unification question
 *   of a space without opening a query, holding its atom argument unevaluated.
 * Guarantees: every claim of the original holds
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

/* (space-contains space atom), answered as a C bool. */
static bool contains(metta *m, const char *space, mt_atom *atom)
{
    return mt_one_truth(mt_eval(m, E("space-contains", mt_spaceref(space), atom)));
}

static mt_list calls(void)
{
    mt_list two = { mt_calloc(2, sizeof(mt_atom *)), 2 };
    require("allocate the batch", two.items != NULL);
    two.items[0] = E("+", 1, 1);
    two.items[1] = E("*", 2, 3);
    return two;
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    mt_space *written = mt_space_open(m, "&written");
    require("open &written", written != NULL);

    /* Stored as written: the space holds the calls. */
    require("store two calls as written", mt_add_all(written, calls()));
    assert(answers_are(mt_atoms(written), E(E("+", 1, 1), E("*", 2, 3))) && "&written holds the calls");

    /* add-reducts evaluates each element on the way in. */
    require("store their answers", mt_one_truth(mt_eval(m, E("add-reducts", mt_spaceref("&reduced"),
                                                             E(E("+", 1, 1), E("*", 2, 3))))));
    assert(answers_are(mt_eval(m, E("get-atoms", mt_spaceref("&reduced"))), E(2, 6))
           && "&reduced holds the answers");
    require("store one answer", mt_one_truth(mt_eval(m, E("add-reduct", mt_spaceref("&single"), E("+", 1, 1)))));
    assert(answers_are(mt_eval(m, E("get-atoms", mt_spaceref("&single"))), E(2)) && "&single holds 2");

    /* A space of facts answers about 2, a space of program about (+ 1 1). */
    assert(answers_are(mt_eval(m, E("match", mt_spaceref("&reduced"), 2, "found")), E("found"))
           && "the reduced space answers a query about 2");
    assert(!mt_first(mt_eval(m, E("match", mt_spaceref("&written"), 2, "found"))) && mt_ok() && "the written one does not");
    assert(answers_are(mt_eval(m, E("match", mt_spaceref("&written"), E("+", 1, 1), "found")), E("found"))
           && "it answers about the call instead");

    /* space-contains holds its atom: (+ 1 1) asks about the expression. */
    assert(contains(m, "&reduced", N(2)) && "2 is in &reduced");
    assert(!contains(m, "&reduced", N(99)) && mt_ok() && "99 is not");
    assert(contains(m, "&written", E("+", 1, 1)) && "(+ 1 1) is in &written");
    assert(!contains(m, "&written", N(2)) && mt_ok() && "2 is not");

    /* It asks about unification, so a stored variable stands for anything. */
    require("store (edge $a $b)", mt_add(written, E("edge", V("a"), V("b"))));
    assert(contains(m, "&written", E("edge", "a", "b")) && "(edge a b) unifies with it");
    assert(contains(m, "&written", E("edge", V("x"), V("y"))) && "so does (edge $x $y)");
    assert(!contains(m, "&written", E("node", "a")) && mt_ok() && "(node a) does not");

    /* A match binds and enumerates; space-contains answers one Bool. */
    assert(answers_are(mt_eval(m, E("match", mt_spaceref("&written"), E("edge", V("x"), V("y")), E(V("x"), V("y")))), E(E(V("x"), V("y"))))
           && "a match answers the binding");
    assert(contains(m, "&written", E("edge", "a", "b")) && "while space-contains only says yes");
    mt_space_close(written);
    mt_close(m);
    return 0;
}
