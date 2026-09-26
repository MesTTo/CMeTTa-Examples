/* Purpose: C's model of NARS's truth functions: a truth value is a struct of
 *   frequency and confidence, and each function is the arithmetic lib_nars
 *   writes, in the order it writes it [source: lib/lib_nars/lib.metta,
 *   Truth_c2w to Truth_Expectation;
 *   commit=8d651070dedaa190e25cc388c029172a63e967be]. A twin computes the
 *   truth value a rule must answer with these and holds the engine's answer
 *   to it exactly, so a rule answering the right term with the wrong truth
 *   is caught in the last bit.
 * Assumes: the includer defines MT_SHORTHAND first; the build does not contract
 *   a multiply and an add into one fused operation, which -std=c11 turns off
 *   in GCC, so each operation rounds once, as SWI-Prolog's float arithmetic
 *   does.
 * Guarantees: each function answers the double the engine answers for the
 *   same operands [tested 2026-09-27T00:35:58+10:00:
 *   make -C extensions/cmetta corpus-check].
 */
#ifndef CH22_NARS_TRUTH_H
#define CH22_NARS_TRUTH_H
#include <cmetta.h>

/* Each MeTTa operator twice, for a body written once over its operators:
   C_X computes in C, and T_X builds the atom (X a b). */
#define C_MIN(a, b) ((a) < (b) ? (a) : (b))
#define C_MAX(a, b) ((a) > (b) ? (a) : (b))

typedef struct truth {
    double f, c;
} truth;

/* The truth the structural rules stand on: one premise and the language's
   own certainty about its shape. */
static const truth nars_default = { 1.0, 0.9 };

/* (stv f c), from two atoms or numbers and from a truth, and the truth an
   (stv f c) atom holds, an integer read as the double it equals. */
#define T_STV(f, c) mt_expr("stv", f, c)
static inline mt_atom *stv(truth t) { return T_STV(t.f, t.c); }
static inline truth stv_truth(const mt_atom *tv) { return (truth){ mt_float(mt_at(tv, 1)), mt_float(mt_at(tv, 2)) }; }

/* The two directions of the evidence map, c = w / (w + 1). */
static inline double w2c(double w) { return w / (w + 1); }
static inline double c2w(double c) { return c / (1 - c); }

/* The probabilistic or, on plain numbers. */
static inline double nars_or(double a, double b) { return 1 - (1 - a) * (1 - b); }

static inline truth deduction(truth a, truth b) { return (truth){ a.f * b.f, (a.f * b.f) * (a.c * b.c) }; }
static inline truth abduction(truth a, truth b) { return (truth){ b.f, w2c((a.f * a.c) * b.c) }; }
static inline truth induction(truth a, truth b) { return abduction(b, a); }
static inline truth exemplification(truth a, truth b) { return (truth){ 1.0, w2c((a.f * b.f) * (a.c * b.c)) }; }
static inline truth negation(truth t) { return (truth){ 1 - t.f, t.c }; }
static inline truth structural_deduction(truth t) { return deduction(t, nars_default); }
static inline truth structural_deduction_negated(truth t) { return negation(structural_deduction(t)); }
static inline truth intersection(truth a, truth b) { return (truth){ a.f * b.f, a.c * b.c }; }
static inline truth structural_intersection(truth t) { return intersection(t, nars_default); }

static inline truth comparison(truth a, truth b)
{
    double f0 = nars_or(a.f, b.f);
    return (truth){ f0 == 0.0 ? 0.0 : (a.f * b.f) / f0, w2c(f0 * (a.c * b.c)) };
}

static inline truth analogy(truth a, truth b) { return (truth){ a.f * b.f, (a.c * b.c) * b.f }; }
static inline truth resemblance(truth a, truth b) { return (truth){ a.f * b.f, (a.c * b.c) * nars_or(a.f, b.f) }; }
static inline truth difference(truth a, truth b) { return (truth){ a.f * (1 - b.f), a.c * b.c }; }

/* The decompositions, named for the polarity of premise, compound and
   conclusion. */
static inline truth decompose_pnn(truth a, truth b)
{
    double fn = a.f * (1 - b.f);
    return (truth){ 1 - fn, fn * (a.c * b.c) };
}
static inline truth decompose_npp(truth a, truth b)
{
    double f = (1 - a.f) * b.f;
    return (truth){ f, f * (a.c * b.c) };
}
static inline truth decompose_pnp(truth a, truth b)
{
    double f = a.f * (1 - b.f);
    return (truth){ f, f * (a.c * b.c) };
}
static inline truth decompose_ppp(truth a, truth b) { return decompose_npp(negation(a), b); }

/* Union and the NNN decomposition answer a bare (f c) pair, not an stv. */
static inline truth truth_union(truth a, truth b) { return (truth){ nars_or(a.f, b.f), a.c * b.c }; }
static inline truth decompose_nnn(truth a, truth b)
{
    double fn = (1 - a.f) * (1 - b.f);
    return (truth){ 1 - fn, fn * (a.c * b.c) };
}

static inline truth eternalize(truth t) { return (truth){ t.f, w2c(t.c) }; }

/* The one rule that adds evidence, and so the one that can raise a
   confidence, which is capped at 0.99: no evidence makes a belief certain. */
static inline truth revision(truth a, truth b)
{
    double w1 = c2w(a.c), w2 = c2w(b.c), w = w1 + w2, f = (w1 * a.f + w2 * b.f) / w, c = w2c(w);
    return (truth){ C_MIN(1.00, f), C_MIN(0.99, C_MAX(C_MAX(c, a.c), b.c)) };
}

static inline double expectation(truth t) { return t.c * (t.f - 0.5) + 0.5; }
#endif
