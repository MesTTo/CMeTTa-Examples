/* Purpose: a declared arrow decides what is evaluated. An argument for a
 *   Number parameter is evaluated before the equation runs and one for an
 *   Atom parameter reaches it as written; a result is not evaluated again,
 *   so wu1 answers the tuple its body built whether its result type is
 *   %Undefined% or Atom, as wu1b shows. C computes each evaluated argument
 *   with its own arithmetic, keeps each held one as the term it built, and
 *   runs wu2's and wu3's bodies as C functions.
 * Guarantees: all five claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

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
    metta *m = open_engine();
    define(m, "wu1", E("->", "Number", "Atom", "%Undefined%"), E(42, V("a"), V("b")));
    define(m, "wu2", E("->", "Number", "Number", "Number"), E("+", V("a"), V("b")));
    define(m, "wu3", E("->", "Number", "Number", "%Undefined%"), E("if", E("<", V("a"), 10), E("+", V("a"), V("b")), E("a", "list", "not", "a", "number")));

    check_answers("an Atom argument stays as written", mt_eval(m, E("wu1", E("+", 2, 4), E("+", 4, 2))), E(42, 2 + 4, E("+", 4, 2)));
    define(m, "wu1b", E("->", "Number", "Atom", "Atom"), E(42, V("a"), V("b")));
    check_answers("and so it does under an Atom result", mt_eval(m, E("wu1b", E("+", 2, 4), E("+", 4, 2))), E(42, 2 + 4, E("+", 4, 2)));
    check_answers("Number arguments are evaluated", mt_eval(m, E("wu2", E("+", 2, 4), E("+", 4, 2))), wu2(2 + 4, 4 + 2));
    check_answers("a result that is no number", mt_eval(m, E("wu3", 42, 0)), wu3(42, 0));
    check_answers("and one that is", mt_eval(m, E("wu3", 2, 0)), wu3(2, 0));
    return done(m);
}
