/* Purpose: the same growth with each round expanding and mating, read
 *   through (superpose (collapse (match ...))). That reading is not the
 *   snapshot it looks like: superpose holds its argument as written and
 *   walks its elements, so its first answer is the symbol collapse, which
 *   expand and mate treat as a term like any other, and the match runs only
 *   when superpose reaches it, after the first answer's whole branch. The
 *   program is the terms it is, the count stays in the engine as the
 *   original counts it, and C's model in matespace.h computes it with that
 *   reading, the symbol collapse leading each round.
 * Guarantees: the original's claim holds [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include "matespace.h"

enum { ROUNDS = 80 };

/* ---- C's model: each round expands, mates and goes on ---- */
static const char *const leading = "collapse";
static void rewrite_k(term_space *s, int64_t n);
static void rewrite_k_next(term_space *s, int64_t n) { rewrite_k(s, n - 1); }
static void mate_then_next(term_space *s, int64_t n) { round_of(s, leading, 'M', mated, n, rewrite_k_next); }
static void rewrite_k(term_space *s, int64_t n)
{
    if (n == 0)
        count_answers(s, n);
    else
        round_of(s, leading, 0, expanded, n, mate_then_next);
}

/* ---- the program ---- */
static mt_atom *num(mt_atom *t) { return E("num", t); }
static mt_atom *no_duplicate(mt_atom *atom) { return E("add-atom-no-duplicate", "&self", atom); }
static mt_atom *read_through(mt_atom *pattern) { return E("superpose", E("collapse", E("match", "&self", pattern, V("t")))); }

int main(void)
{
    metta *m = open_engine();
    mt_atom *program[] = {
        E("=", E("add-atom-no-duplicate", V("Space"), V("Atom")),
          E("if", E("==", mt_unit(), E("collapse", E("once", E("match", V("Space"), V("Atom"), V("Atom"))))), E("add-atom", V("Space"), V("Atom")),
            E("empty"))),
        E("=", E("expand"),
          E("case", read_through(num(V("t"))), E(E(V("t"), E(no_duplicate(num(E("M", V("t")))), no_duplicate(num(E("W", V("t"))))))))),
        E("=", E("mate"),
          E("case", read_through(num(E("M", V("t")))),
            E(E(V("t"), E("case", E("once", E("match", "&self", num(E("W", V("t"))), V("t"))), E(E(V("t"), no_duplicate(num(E("C", V("t"))))))))))),
        E("=", E("rewriteK", V("n")),
          E("if", E("==", V("n"), 0), "done",
            E("let*", E(E(V("temp1"), E("expand")), E(V("temp2"), E("mate"))), E("rewriteK", E("-", V("n"), 1))))),
        E("=", E("mate-space-demo", V("K")),
          E("let*", E(E(V("s"), E("add-atom", "&self", num(S("Z")))), E(V("g"), E("rewriteK", V("K")))),
            E("match", "&self", num(V("1")), num(V("1"))))),
    };
    for (size_t i = 0; i < sizeof program / sizeof *program; i++) require("an equation", mt_add(m, program[i]));

    term_space model = { NULL, 0, NULL, 0 };
    require("the seed", term_add(&model, "Z"));
    rewrite_k(&model, ROUNDS);
    check_answers("the final match's answers over every branch",
                  mt_eval(m, E("length", E("collapse", E("mate-space-demo", ROUNDS)))), model.answers);
    term_space_free(&model);
    return done(m);
}
