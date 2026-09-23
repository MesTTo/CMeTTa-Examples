/* Purpose: answers in, or programs in. mt_add_all() stores atoms as
 *   written, so &written holds calls; add-reducts evaluates each first, so
 *   &reduced holds answers; and space-contains asks one unification question
 *   of a space without opening a query, holding its atom argument unevaluated.
 * Guarantees: every claim of the original holds [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

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
    metta *m = open_engine();
    mt_space *written = mt_space_open(m, "&written");
    require("open &written", written != NULL);

    /* Stored as written: the space holds the calls. */
    require("store two calls as written", mt_add_all(written, calls()));
    check_answers("&written holds the calls", mt_atoms(written), E("+", 1, 1), E("*", 2, 3));

    /* add-reducts evaluates each element on the way in. */
    require("store their answers", mt_one_truth(mt_eval(m, E("add-reducts", mt_spaceref("&reduced"),
                                                             E(E("+", 1, 1), E("*", 2, 3))))));
    check_answers("&reduced holds the answers",
                  mt_eval(m, E("get-atoms", mt_spaceref("&reduced"))), 2, 6);
    require("store one answer", mt_one_truth(mt_eval(m, E("add-reduct", mt_spaceref("&single"), E("+", 1, 1)))));
    check_answers("&single holds 2", mt_eval(m, E("get-atoms", mt_spaceref("&single"))), 2);

    /* A space of facts answers about 2, a space of program about (+ 1 1). */
    check_answers("the reduced space answers a query about 2",
                  mt_eval(m, E("match", mt_spaceref("&reduced"), 2, "found")), "found");
    check_none("the written one does not", mt_eval(m, E("match", mt_spaceref("&written"), 2, "found")));
    check_answers("it answers about the call instead",
                  mt_eval(m, E("match", mt_spaceref("&written"), E("+", 1, 1), "found")), "found");

    /* space-contains holds its atom: (+ 1 1) asks about the expression. */
    check("2 is in &reduced", contains(m, "&reduced", N(2)));
    check("99 is not", !contains(m, "&reduced", N(99)) && mt_ok());
    check("(+ 1 1) is in &written", contains(m, "&written", E("+", 1, 1)));
    check("2 is not", !contains(m, "&written", N(2)) && mt_ok());

    /* It asks about unification, so a stored variable stands for anything. */
    require("store (edge $a $b)", mt_add(written, E("edge", V("a"), V("b"))));
    check("(edge a b) unifies with it", contains(m, "&written", E("edge", "a", "b")));
    check("so does (edge $x $y)", contains(m, "&written", E("edge", V("x"), V("y"))));
    check("(node a) does not", !contains(m, "&written", E("node", "a")) && mt_ok());

    /* A match binds and enumerates; space-contains answers one Bool. */
    check_answers("a match answers the binding",
                  mt_eval(m, E("match", mt_spaceref("&written"), E("edge", V("x"), V("y")), E(V("x"), V("y")))),
                  E(V("x"), V("y")));
    check("while space-contains only says yes", contains(m, "&written", E("edge", "a", "b")));
    mt_space_close(written);
    return done(m);
}
