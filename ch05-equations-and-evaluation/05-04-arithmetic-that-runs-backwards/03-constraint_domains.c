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
 * Guarantees: all eleven claims of the original hold
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

#define WHERE(cond, answer) E("let", B(true), (cond), (answer))

static mt_atom *clpq(mt_atom *constraint) { return E("clpq", constraint); }
static mt_atom *clpb(mt_atom *formula) { return E("clpb", formula); }

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_constraints",
            mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_constraints")))));

    /* CLP(Q): exact rationals, which clpfd has no answer for. */
    mt_atom *half = mt_one(mt_eval(m, WHERE(clpq(E("=", E("*", 2, V("x")), 1)), V("x"))));
    mt_ratio ratio = mt_ratio_of(half);
    assert(ratio.num == 1 && ratio.den == 2 && "2x = 1 has the exact solution 1/2");
    mt_drop(half);
    assert(answers_are(mt_eval(m, WHERE(clpq(E("=", E("*", 2, V("x")), 1)), E("*", 2, V("x")))), E(1)) && "and twice it is 1");

    assert(answers_are(mt_eval(m, WHERE(clpq(E(">=", V("a"), 0)), E("clpq-entailed", E(">=", V("a"), 0)))), E(B(true)))
           && "a posted bound entails itself");
    assert(answers_are(mt_eval(m, WHERE(clpq(E(">=", V("b"), 0)), E("clpq-entailed", E(">=", V("b"), 5)))), E(B(false)))
           && "but not a tighter one");
    assert(!mt_first(mt_eval(m, WHERE(clpq(E("=", V("c"), 1)), clpq(E("=", V("c"), 2))))) && mt_ok()
           && "a contradiction has no answer");
    assert(answers_are(mt_eval(m, WHERE(clpq(E("=", V("d"), 1)),
                                        WHERE(clpq(E("=", V("e"), 2)), clpq(E("=\\=", V("d"), V("e")))))), E(B(true)))
           && "a disequation over the rationals");
    assert(answers_are(mt_eval(m, WHERE(clpq(E(">=", V("f"), 0)), WHERE(clpq(E("=<", V("f"), 3)),
                                                                        E("residual-goals", V("f"))))), E(E(E("{}", E(",", E(">=", V("g"), 0), E("=<", V("g"), 3))))))
           && "what an answer still carries");

    /* CLP(B): exactly one of two, labelled; then two formulas decided. */
    assert(answers_are(mt_eval(m, WHERE(clpb(E("card", E(1), E(V("m"), V("n")))), E("clpb-labeling", E(V("m"), V("n"))))), E(E(0, 1), E(1, 0)))
           && "exactly one of m and n");
    assert(answers_are(mt_eval(m, E("clpb-taut", E("+", V("t"), E("~", V("t"))))), E(B(true))) && "t or not t is a tautology");
    assert(answers_are(mt_eval(m, E("clpb-taut", E("*", V("u"), E("~", V("u"))))), E(B(false))) && "u and not u is not");

    assert(answers_are(mt_eval(m, E("if", E("and", E("or", V("x"), B(true)), V("y")), E(V("x"), V("y")))), E(E(B(true), B(true)), E(B(false), B(true))))
           && "and, or and not enumerate on their own");
    mt_close(m);
    return 0;
}
