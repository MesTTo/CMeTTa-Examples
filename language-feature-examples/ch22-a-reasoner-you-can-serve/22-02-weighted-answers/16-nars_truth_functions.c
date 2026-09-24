/* Purpose: NARS's truth functions, one call each, with C's model of each in
 *   nars_truth.h computing what the call must answer, so every claim holds
 *   the engine's arithmetic to C's in the last bit. The rules over truths are
 *   one table of name, C function and operands, each row saying whether the
 *   rule answers an (stv f c) or, as union and the NNN decomposition do, a
 *   bare (f c) pair; the three that answer a number are rows of their own.
 *   Where the original compares two of the engine's answers with ==, C
 *   compares the two atoms the engine handed it, and where it allows a
 *   float's error, C measures the distance with fabs.
 * Guarantees: all forty-two claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include "nars_truth.h"
#include <math.h>

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
    metta *m = open_engine();
    require("lib_nars", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_nars")))));

    /* The evidence map both ways: 9 units of evidence is confidence 0.9, so
       0.9 converts back to 9 within a float's error, and so does a round
       trip. */
    static const double weights[] = { 9.0, 0.0, 1.0 };
    for (size_t i = 0; i < 3; i++) check_answers("Truth_w2c", mt_eval(m, E("Truth_w2c", weights[i])), w2c(weights[i]));
    check("Truth_c2w inverts w2c at 0.9", fabs(mt_one_float(mt_eval(m, E("Truth_c2w", 0.9))) - weights[0]) < 1.0e-9);
    check_answers("Truth_c2w", mt_eval(m, E("Truth_c2w", 0.0)), c2w(0.0));
    check("w2c after c2w is the identity",
          fabs(mt_one_float(mt_eval(m, E("Truth_w2c", E("Truth_c2w", 0.75)))) - 0.75) < 1.0e-9);

    for (size_t i = 0; i < sizeof rules / sizeof *rules; i++) {
        const rule *r = &rules[i];
        truth t = r->two ? r->two(r->a, r->b) : r->one(r->a);
        check_answers(r->name, mt_eval(m, call(r)), r->pair ? E(t.f, t.c) : stv(t));
    }

    /* Induction is abduction with the operands swapped, the structural rules
       are the binary ones against the default, and PPP is NPP of the
       negation: each is two of the engine's answers, compared in C. */
    truth half = { 0.5, 0.9 }, strong = { 1.0, 0.8 };
    check_atom("induction is abduction swapped", mt_one(ask(m, "Truth_Induction", half, strong)),
               mt_one(ask(m, "Truth_Abduction", strong, half)));
    check_atom("structural deduction is deduction against the default",
               mt_one(mt_eval(m, E("Truth_StructuralDeduction", stv(half)))), mt_one(ask(m, "Truth_Deduction", half, nars_default)));
    check_atom("PPP is NPP of the negation", mt_one(ask(m, "Truth_DecomposePPP", nars_default, nars_default)),
               mt_one(mt_eval(m, E("Truth_DecomposeNPP", E("Truth_Negation", stv(nars_default)), stv(nars_default)))));

    /* The probabilistic or is the one member that takes and answers numbers;
       the expectation projects a truth onto [0,1]. */
    static const double ors[][2] = { { 0.5, 0.5 }, { 0.0, 0.0 }, { 1.0, 0.0 } };
    for (size_t i = 0; i < 3; i++) check_answers("Truth_or", mt_eval(m, E("Truth_or", ors[i][0], ors[i][1])), nars_or(ors[i][0], ors[i][1]));
    static const truth expected[] = { { 1.0, 0.9 }, { 0.0, 0.9 }, { 0.5, 0.9 }, { 1.0, 0.0 } };
    for (size_t i = 0; i < 4; i++)
        check_answers("Truth_Expectation", mt_eval(m, E("Truth_Expectation", stv(expected[i]))), expectation(expected[i]));
    return done(m);
}
