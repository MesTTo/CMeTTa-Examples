/* Purpose: NARS's truth functions, one call each, with C's model of each in
 *   nars_truth.h computing what the call must answer, so every claim holds
 *   the engine's arithmetic to C's in the last bit. The rules over truths are
 *   one table of name, C function and operands, each row saying whether the
 *   rule answers an (stv f c) or, as union and the NNN decomposition do, a
 *   bare (f c) pair; the three that answer a number are rows of their own.
 *   Where the original compares two of the engine's answers with ==, C
 *   compares the two atoms the engine handed it, and where it allows a
 *   float's error, C measures the distance with fabs.
 * Guarantees: all forty-two claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include "_fixtures/nars_truth.h"
#include <math.h>

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

/* A rule over truths: B absent for the one-premise rules. */
typedef struct rule {
    const char *name;
    truth (*two)(truth, truth);
    truth (*one)(truth);
    truth a, b;
    bool pair; /* answers a bare (f c) rather than an stv */
} rule;

#define TWO(name, fn, a, b) { name, fn, NULL, a, b, false }
#define ONE(name, fn, a) { name, NULL, fn, a, { 0, 0 }, false }
#define PAIR(name, fn, a, b) { name, fn, NULL, a, b, true }

#define STV(f, c) { f, c }
static const rule rules[] = {
    TWO("Truth_Deduction", deduction, STV(1.0, 0.9), STV(1.0, 0.9)),
    TWO("Truth_Deduction", deduction, STV(0.5, 0.9), STV(1.0, 0.9)),
    TWO("Truth_Abduction", abduction, STV(1.0, 0.9), STV(1.0, 0.9)),
    TWO("Truth_Induction", induction, STV(1.0, 0.9), STV(1.0, 0.9)),
    TWO("Truth_Exemplification", exemplification, STV(1.0, 0.9), STV(1.0, 0.9)),
    TWO("Truth_Exemplification", exemplification, STV(0.2, 0.9), STV(0.2, 0.9)),
    ONE("Truth_Negation", negation, STV(1.0, 0.9)),
    ONE("Truth_Negation", negation, STV(0.0, 0.9)),
    ONE("Truth_StructuralDeduction", structural_deduction, STV(1.0, 0.9)),
    ONE("Truth_StructuralDeductionNegated", structural_deduction_negated, STV(1.0, 0.9)),
    ONE("Truth_StructuralIntersection", structural_intersection, STV(0.8, 0.9)),
    TWO("Truth_Intersection", intersection, STV(0.8, 0.9), STV(0.5, 0.9)),
    TWO("Truth_Comparison", comparison, STV(1.0, 0.9), STV(1.0, 0.9)),
    TWO("Truth_Comparison", comparison, STV(0.0, 0.9), STV(0.0, 0.9)),
    TWO("Truth_Resemblance", resemblance, STV(1.0, 0.9), STV(0.5, 0.9)),
    TWO("Truth_Analogy", analogy, STV(1.0, 0.9), STV(0.5, 0.9)),
    TWO("Truth_Difference", difference, STV(1.0, 0.9), STV(0.25, 0.9)),
    TWO("Truth_DecomposePNN", decompose_pnn, STV(1.0, 0.9), STV(0.0, 0.9)),
    TWO("Truth_DecomposeNPP", decompose_npp, STV(0.0, 0.9), STV(1.0, 0.9)),
    TWO("Truth_DecomposePNP", decompose_pnp, STV(1.0, 0.9), STV(0.0, 0.9)),
    PAIR("Truth_Union", truth_union, STV(0.5, 0.9), STV(0.5, 0.9)),
    PAIR("Truth_DecomposeNNN", decompose_nnn, STV(0.0, 0.9), STV(0.0, 0.9)),
    ONE("Truth_Eternalize", eternalize, STV(0.8, 0.9)),
    TWO("Truth_Revision", revision, STV(1.0, 0.9), STV(0.0, 0.9)),
    TWO("Truth_Revision", revision, STV(1.0, 0.5), STV(1.0, 0.5)),
    TWO("Truth_Revision", revision, STV(1.0, 0.99), STV(1.0, 0.99)),
};

/* (NAME stv...) for a rule's operands. */
static mt_atom *call(const rule *r)
{
    return r->two ? E(r->name, stv(r->a), stv(r->b)) : E(r->name, stv(r->a));
}

static mt_answers *ask(metta *m, const char *name, truth a, truth b) { return mt_eval(m, E(name, stv(a), stv(b))); }

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("lib_nars", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_nars")))));

    /* The evidence map both ways: 9 units of evidence is confidence 0.9, so
       0.9 converts back to 9 within a float's error, and so does a round
       trip. */
    static const double weights[] = { 9.0, 0.0, 1.0 };
    for (size_t i = 0; i < 3; i++) assert(answers_are(mt_eval(m, E("Truth_w2c", weights[i])), E(w2c(weights[i]))) && "Truth_w2c");
    assert(fabs(mt_one_float(mt_eval(m, E("Truth_c2w", 0.9))) - weights[0]) < 1.0e-9 && "Truth_c2w inverts w2c at 0.9");
    assert(answers_are(mt_eval(m, E("Truth_c2w", 0.0)), E(c2w(0.0))) && "Truth_c2w");
    assert(fabs(mt_one_float(mt_eval(m, E("Truth_w2c", E("Truth_c2w", 0.75)))) - 0.75) < 1.0e-9
           && "w2c after c2w is the identity");

    for (size_t i = 0; i < sizeof rules / sizeof *rules; i++) {
        const rule *r = &rules[i];
        truth t = r->two ? r->two(r->a, r->b) : r->one(r->a);
        assert(answers_are(mt_eval(m, call(r)), E(r->pair ? E(t.f, t.c) : stv(t))) && r->name);
    }

    /* Induction is abduction with the operands swapped, the structural rules
       are the binary ones against the default, and PPP is NPP of the
       negation: each is two of the engine's answers, compared in C. */
    truth half = { 0.5, 0.9 }, strong = { 1.0, 0.8 };
    assert(atom_is(mt_one(ask(m, "Truth_Induction", half, strong)), mt_one(ask(m, "Truth_Abduction", strong, half)))
           && "induction is abduction swapped");
    assert(atom_is(mt_one(mt_eval(m, E("Truth_StructuralDeduction", stv(half)))), mt_one(ask(m, "Truth_Deduction", half, nars_default)))
           && "structural deduction is deduction against the default");
    assert(atom_is(mt_one(ask(m, "Truth_DecomposePPP", nars_default, nars_default)), mt_one(mt_eval(m, E("Truth_DecomposeNPP", E("Truth_Negation", stv(nars_default)), stv(nars_default)))))
           && "PPP is NPP of the negation");

    /* The probabilistic or is the one member that takes and answers numbers;
       the expectation projects a truth onto [0,1]. */
    static const double ors[][2] = { { 0.5, 0.5 }, { 0.0, 0.0 }, { 1.0, 0.0 } };
    for (size_t i = 0; i < 3; i++) assert(answers_are(mt_eval(m, E("Truth_or", ors[i][0], ors[i][1])), E(nars_or(ors[i][0], ors[i][1]))) && "Truth_or");
    static const truth expected[] = { { 1.0, 0.9 }, { 0.0, 0.9 }, { 0.5, 0.9 }, { 1.0, 0.0 } };
    for (size_t i = 0; i < 4; i++)
        assert(answers_are(mt_eval(m, E("Truth_Expectation", stv(expected[i]))), E(expectation(expected[i]))) && "Truth_Expectation");
    mt_close(m);
    return 0;
}
