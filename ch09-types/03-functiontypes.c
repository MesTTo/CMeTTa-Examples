/* Purpose: a declared arrow decides what is evaluated. An argument for a
 *   Number parameter is evaluated before the equation runs and one for an
 *   Atom parameter reaches it as written; a result is not evaluated again,
 *   so wu1 answers the tuple its body built whether its result type is
 *   %Undefined% or Atom, as wu1b shows. C computes each evaluated argument
 *   with its own arithmetic, keeps each held one as the term it built, and
 *   runs wu2's and wu3's bodies as C functions.
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

/* wu2's and wu3's bodies in C: a sum, and a sum only below 10. */
static mt_atom *wu2(int64_t a, int64_t b) { return N(a + b); }
static mt_atom *wu3(int64_t a, int64_t b) { return a < 10 ? N(a + b) : E("a", "list", "not", "a", "number"); }

/* (: name arrow) and (= (name $a $b) body), in the original's order. */
static void define(metta *m, const char *name, mt_atom *arrow, mt_atom *body)
{
    require("declare the arrow", mt_add(m, E(":", name, arrow)));
    require("define the equation", mt_add(m, E("=", E(name, V("a"), V("b")), body)));
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    define(m, "wu1", E("->", "Number", "Atom", "%Undefined%"), E(42, V("a"), V("b")));
    define(m, "wu2", E("->", "Number", "Number", "Number"), E("+", V("a"), V("b")));
    define(m, "wu3", E("->", "Number", "Number", "%Undefined%"), E("if", E("<", V("a"), 10), E("+", V("a"), V("b")), E("a", "list", "not", "a", "number")));

    assert(answers_are(mt_eval(m, E("wu1", E("+", 2, 4), E("+", 4, 2))), E(E(42, 2 + 4, E("+", 4, 2)))) && "an Atom argument stays as written");
    define(m, "wu1b", E("->", "Number", "Atom", "Atom"), E(42, V("a"), V("b")));
    assert(answers_are(mt_eval(m, E("wu1b", E("+", 2, 4), E("+", 4, 2))), E(E(42, 2 + 4, E("+", 4, 2)))) && "and so it does under an Atom result");
    assert(answers_are(mt_eval(m, E("wu2", E("+", 2, 4), E("+", 4, 2))), E(wu2(2 + 4, 4 + 2))) && "Number arguments are evaluated");
    assert(answers_are(mt_eval(m, E("wu3", 42, 0)), E(wu3(42, 0))) && "a result that is no number");
    assert(answers_are(mt_eval(m, E("wu3", 2, 0)), E(wu3(2, 0))) && "and one that is");
    mt_close(m);
    return 0;
}
