/* Purpose: Peano addition run every way. peano() builds a numeral with a C
 *   loop; plus is two equations; mt_solve runs it backwards for either
 *   operand, reading the unknown by name, and for both at once, where C's
 *   loop over a from 0 to 4 gives the pairs it expects; mt_first keeps the
 *   first pair, as once does.
 * Guarantees: all five claims of the original hold
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

/* Whether an atom is want up to renaming variables; takes both, and shows
   them when they differ. */
static inline bool atom_is(mt_atom *got, mt_atom *want)
{
    bool holds = got && want && mt_alpha_eq(got, want);
    if (!holds) fprintf(stderr, "  got %s\n  want %s\n", got ? mt_show(got) : "nothing",
                        want ? mt_show(want) : "nothing");
    mt_drop(got);
    mt_drop(want);
    return holds;
}

/* (S (S ... Z)), n deep. */
static mt_atom *peano(int n)
{
    mt_atom *numeral = S("Z");
    while (n-- > 0) numeral = E("S", numeral);
    return numeral;
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("(= (plus Z $y) $y)", mt_add(m, E("=", E("plus", "Z", V("y")), V("y"))));
    require("(= (plus (S $x) $y) (S (plus $x $y)))",
            mt_add(m, E("=", E("plus", E("S", V("x")), V("y")), E("S", E("plus", V("x"), V("y"))))));

    assert(answers_are(mt_eval(m, E("plus", peano(2), peano(1))), E(peano(3))) && "2 + 1 is 3");
    assert(list_is(mt_all(mt_solve(m, peano(4), E("plus", V("A"), peano(1)))), E(peano(3))) && "A + 1 = 4 solves A");
    assert(list_is(mt_all(mt_solve(m, peano(4), E("plus", peano(1), V("B")))), E(peano(3))) && "1 + B = 4 solves B");

    mt_atom *pairs[5];
    for (int a = 0; a <= 4; a++) pairs[a] = E(peano(a), peano(4 - a));
    assert(list_is(mt_all(mt_solve(m, peano(4), E("plus", V("A"), V("B")))), mt_exprv(5, pairs)) && "A + B = 4 answers every pair");
    assert(atom_is(mt_first(mt_solve(m, peano(4), E("plus", V("A"), V("B")))), E(peano(0), peano(4))) && "the first pair only");
    mt_close(m);
    return 0;
}
