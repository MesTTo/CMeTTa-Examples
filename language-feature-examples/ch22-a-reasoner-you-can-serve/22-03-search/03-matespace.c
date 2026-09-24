/* Purpose: a space grown to a million answers by rewriting: expand doubles
 *   every num atom into an M-branch and a W-branch, expandK does that 390
 *   times, mate pairs the branches, and the final match answers every num
 *   atom on every branch. The program is the terms it is, and the count
 *   stays in the engine as the original counts it, one integer crossing to
 *   C. C's model of the program in matespace.h computes that integer: the
 *   same rules over C strings, in the order the engine evaluates them,
 *   each lazy match walking the atoms present when it was called and each
 *   answer running the rest of the program before the next.
 * Guarantees: the original's claim holds [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include "matespace.h"

enum { ROUNDS = 390 };

/* ---- C's model: expandK, then mate, then the final match ---- */
static void count_after_mate(term_space *s, int64_t n) { round_of(s, NULL, 'M', mated, n, count_answers); }
static void expand_k(term_space *s, int64_t n);
static void expand_k_next(term_space *s, int64_t n) { expand_k(s, n - 1); }
static void expand_k(term_space *s, int64_t n)
{
    if (n == 0)
        count_after_mate(s, n);
    else
        round_of(s, NULL, 0, expanded, n, expand_k_next);
}

/* ---- the program ---- */
static mt_atom *num(mt_atom *t) { return E("num", t); }
static mt_atom *no_duplicate(mt_atom *atom) { return E("add-atom-no-duplicate", "&self", atom); }

int main(void)
{
    metta *m = open_engine();
    mt_atom *program[] = {
        E("=", E("add-atom-no-duplicate", V("Space"), V("Atom")),
          E("if", E("==", mt_unit(), E("collapse", E("once", E("match", V("Space"), V("Atom"), V("Atom"))))), E("add-atom", V("Space"), V("Atom")),
            E("empty"))),
        E("=", E("expand"),
          E("case", E("match", "&self", num(V("t")), V("t")),
            E(E(V("t"), E(no_duplicate(num(E("M", V("t")))), no_duplicate(num(E("W", V("t"))))))))),
        E("=", E("mate"),
          E("case", E("match", "&self", num(E("M", V("t"))), V("t")),
            E(E(V("t"), E("case", E("once", E("match", "&self", num(E("W", V("t"))), V("t"))), E(E(V("t"), no_duplicate(num(E("C", V("t"))))))))))),
        E("=", E("expandK", V("n")), E("if", E("==", V("n"), 0), "done", E("let", V("temp1"), E("expand"), E("expandK", E("-", V("n"), 1))))),
        E("=", E("mate-space-demo", V("K")),
          E("let*", E(E(V("s"), E("add-atom", "&self", num(S("Z")))), E(V("g"), E("expandK", V("K"))), E(V("h"), E("mate"))),
            E("match", "&self", num(V("1")), num(V("1"))))),
    };
    for (size_t i = 0; i < sizeof program / sizeof *program; i++) require("an equation", mt_add(m, program[i]));

    term_space model = { NULL, 0, NULL, 0 };
    require("the seed", term_add(&model, "Z"));
    expand_k(&model, ROUNDS);
    check_answers("the final match's answers over every branch",
                  mt_eval(m, E("length", E("collapse", E("mate-space-demo", ROUNDS)))), model.answers);
    term_space_free(&model);
    return done(m);
}
