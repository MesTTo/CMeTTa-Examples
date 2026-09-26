/* Purpose: a space grown to a million answers by rewriting: expand doubles
 *   every num atom into an M-branch and a W-branch, expandK does that 390
 *   times, mate pairs the branches, and the final match answers every num
 *   atom on every branch. The program is the terms it is, and the count
 *   stays in the engine as the original counts it, one integer crossing to
 *   C. C's model of the program in matespace.h computes that integer: the
 *   same rules over C strings, in the order the engine evaluates them,
 *   each lazy match walking the atoms present when it was called and each
 *   answer running the rest of the program before the next.
 * Guarantees: the original's claim holds [tested 2026-09-27T00:35:58+10:00:
 *   make -C extensions/cmetta corpus-check].
 */
#define _POSIX_C_SOURCE 200809L
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include "_fixtures/matespace.h"

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
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
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
    assert(answers_are(mt_eval(m, E("length", E("collapse", E("mate-space-demo", ROUNDS)))), E(model.answers))
           && "the final match's answers over every branch");
    term_space_free(&model);
    mt_close(m);
    return 0;
}
