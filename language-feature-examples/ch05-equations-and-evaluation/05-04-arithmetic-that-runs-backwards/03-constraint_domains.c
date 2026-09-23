/* Purpose: the other two constraint domains, each one entry point, asked in
 *   guard terms, (let True constraint question), so what a constraint posts
 *   is in force for the question. CLP(Q) solves 2x = 1 and C reads the exact
 *   answer as the ratio 1/2 with mt_ratio_of, where MeTTa source needs repr
 *   because its reader has no rational literal; it decides entailment, fails
 *   on a contradiction and keeps a disequation. The constraints an answer
 *   still carries come back through residual-goals as a Prolog curly term,
 *   which arrives in the wire grammar as the expression ({} (, ...)) and
 *   compares as a term, since a C expectation is data and never evaluated,
 *   where MeTTa source reads it through repr. CLP(B) labels
 *   "exactly one of two" and decides tautologies, and the engine's own and,
 *   or and not enumerate the same way without it.
 * Guarantees: all eleven claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

#define WHERE(cond, answer) E("let", B(true), (cond), (answer))

static mt_atom *clpq(mt_atom *constraint) { return E("clpq", constraint); }
static mt_atom *clpb(mt_atom *formula) { return E("clpb", formula); }

int main(void)
{
    metta *m = open_engine();
    require("import lib_constraints",
            mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_constraints")))));

    /* CLP(Q): exact rationals, which clpfd has no answer for. */
    mt_atom *half = mt_one(mt_eval(m, WHERE(clpq(E("=", E("*", 2, V("x")), 1)), V("x"))));
    mt_ratio ratio = mt_ratio_of(half);
    check("2x = 1 has the exact solution 1/2", ratio.num == 1 && ratio.den == 2);
    mt_drop(half);
    check_answers("and twice it is 1", mt_eval(m, WHERE(clpq(E("=", E("*", 2, V("x")), 1)), E("*", 2, V("x")))), 1);

    check_answers("a posted bound entails itself",
                  mt_eval(m, WHERE(clpq(E(">=", V("a"), 0)), E("clpq-entailed", E(">=", V("a"), 0)))), B(true));
    check_answers("but not a tighter one",
                  mt_eval(m, WHERE(clpq(E(">=", V("b"), 0)), E("clpq-entailed", E(">=", V("b"), 5)))), B(false));
    check_none("a contradiction has no answer",
               mt_eval(m, WHERE(clpq(E("=", V("c"), 1)), clpq(E("=", V("c"), 2)))));
    check_answers("a disequation over the rationals",
                  mt_eval(m, WHERE(clpq(E("=", V("d"), 1)),
                                   WHERE(clpq(E("=", V("e"), 2)), clpq(E("=\\=", V("d"), V("e")))))), B(true));
    check_answers("what an answer still carries",
                  mt_eval(m, WHERE(clpq(E(">=", V("f"), 0)), WHERE(clpq(E("=<", V("f"), 3)),
                                                                   E("residual-goals", V("f"))))),
                  E(E("{}", E(",", E(">=", V("g"), 0), E("=<", V("g"), 3)))));

    /* CLP(B): exactly one of two, labelled; then two formulas decided. */
    check_answers("exactly one of m and n",
                  mt_eval(m, WHERE(clpb(E("card", E(1), E(V("m"), V("n")))), E("clpb-labeling", E(V("m"), V("n"))))),
                  E(0, 1), E(1, 0));
    check_answers("t or not t is a tautology", mt_eval(m, E("clpb-taut", E("+", V("t"), E("~", V("t"))))), B(true));
    check_answers("u and not u is not", mt_eval(m, E("clpb-taut", E("*", V("u"), E("~", V("u"))))), B(false));

    check_answers("and, or and not enumerate on their own",
                  mt_eval(m, E("if", E("and", E("or", V("x"), B(true)), V("y")), E(V("x"), V("y")))),
                  E(B(true), B(true)), E(B(false), B(true)));
    return done(m);
}
