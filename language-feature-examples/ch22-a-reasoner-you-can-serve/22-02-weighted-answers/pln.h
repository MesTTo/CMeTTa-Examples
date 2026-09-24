/* Purpose: C's model of PLN's truth formulas, from their two homes. The
 *   consistency conditions and the deduction that 03 and 04 define in the
 *   program itself are one body each over lowering.h's operators: expanded
 *   with C's they are the functions a twin computes with, and with the atom
 *   builders they are the equations install_deduction() adds, so the program
 *   the engine runs and the model C checks it against cannot differ. The
 *   formulas lib_pln ships divide through /safe, which answers nothing for a
 *   denominator that is not positive, and each such C function answers
 *   whether it has a value and writes it, so a caller composing them stops at
 *   the first that has none, which is how an argument that answers nothing
 *   empties a MeTTa call [source: lib/lib_pln/lib.metta, clamp to
 *   Truth_evaluationImplication; commit=8d651070dedaa190e25cc388c029172a63e967be].
 * Assumes: the includer includes common.h and nars_truth.h first, whose
 *   note on rounding holds here too.
 * Guarantees: each function answers the value the engine answers for the
 *   same operands, and answers none exactly where the engine answers none
 *   [tested: make twins; commit=WORKTREE].
 */
#ifndef CH22_PLN_H
#define CH22_PLN_H
#include "lowering.h"

/* ---- the consistency conditions, one body each ---- */
#define CLAMP(MIN, MAX, v, lo, hi) MIN(hi, MAX(v, lo))
#define SMALLEST(ADD, SUB, DIV, CLAMP_, As, Bs) CLAMP_(DIV(SUB(ADD(As, Bs), 1), As), 0, 1)
#define LARGEST(DIV, CLAMP_, As, Bs) CLAMP_(DIV(Bs, As), 0, 1)
#define CONSISTENT(AND, LT, LE, SMALLEST_, LARGEST_, As, Bs, ABs) \
    AND(LT(0, As), AND(LE(SMALLEST_(As, Bs), ABs), LE(ABs, LARGEST_(As, Bs))))

/* The deduction formula: the strength through the conditional
   probabilities, or R's own where Q is nearly certain; the weakest
   confidence; and (stv 1 0) where the premises are inconsistent. */
#define DEDUCTION(IF, AND, LT, ADD, SUB, MUL, DIV, MIN, STV, CONSISTENT_, Ps, Pc, Qs, Qc, Rs, Rc, PQs, PQc, QRs, QRc) \
    IF(AND(CONSISTENT_(Ps, Qs, PQs), CONSISTENT_(Qs, Rs, QRs)),                                                      \
       STV(IF(LT(0.9999, Qs), Rs, ADD(MUL(PQs, QRs), DIV(MUL(SUB(1, PQs), SUB(Rs, MUL(Qs, QRs))), SUB(1, Qs)))),     \
           MIN(Pc, MIN(Qc, MIN(Rc, MIN(PQc, QRc))))),                                                              \
       STV(1, 0))

#define T_CLAMP(v, lo, hi) mt_expr("clamp", v, lo, hi)
#define T_SMALLEST(As, Bs) mt_expr("smallest-intersection-probability", As, Bs)
#define T_LARGEST(As, Bs) mt_expr("largest-intersection-probability", As, Bs)
#define T_CONSISTENT(As, Bs, ABs) mt_expr("conditional-probability-consistency", As, Bs, ABs)

static inline double clamp(double v, double lo, double hi) { return CLAMP(C_MIN, C_MAX, v, lo, hi); }
static inline double smallest_intersection(double As, double Bs) { return SMALLEST(C_ADD, C_SUB, C_DIV, clamp, As, Bs); }
static inline double largest_intersection(double As, double Bs) { return LARGEST(C_DIV, clamp, As, Bs); }
static inline bool consistent(double As, double Bs, double ABs)
{
    return CONSISTENT(C_AND, C_LT, C_LE, smallest_intersection, largest_intersection, As, Bs, ABs);
}

/* The (stv ...) the program's own Truth_Deduction answers. */
static inline mt_atom *deduction_answer(truth P, truth Q, truth R, truth PQ, truth QR)
{
    return DEDUCTION(C_IF, C_AND, C_LT, C_ADD, C_SUB, C_MUL, C_DIV, C_MIN, T_STV, consistent, P.f, P.c, Q.f, Q.c, R.f, R.c,
                     PQ.f, PQ.c, QR.f, QR.c);
}

/* The consistency conditions and the deduction as 03 and 04 define them:
   each type declaration, then each equation from its body. */
static inline void install_deduction(metta *m)
{
    mt_atom *number = mt_sym("Number");
    mt_atom *a = mt_var("As"), *b = mt_var("Bs"), *ab = mt_var("ABs");
    mt_atom *definitions[] = {
        mt_expr("=", T_CLAMP(mt_var("v"), mt_var("min"), mt_var("max")),
                CLAMP(T_MIN, T_MAX, mt_var("v"), mt_var("min"), mt_var("max"))),
        mt_expr(":", "smallest-intersection-probability", mt_expr("->", mt_keep(number), mt_keep(number), mt_keep(number))),
        mt_expr("=", T_SMALLEST(mt_keep(a), mt_keep(b)), SMALLEST(T_ADD, T_SUB, T_DIV, T_CLAMP, mt_keep(a), mt_keep(b))),
        mt_expr(":", "largest-intersection-probability", mt_expr("->", mt_keep(number), mt_keep(number), mt_keep(number))),
        mt_expr("=", T_LARGEST(mt_keep(a), mt_keep(b)), LARGEST(T_DIV, T_CLAMP, mt_keep(a), mt_keep(b))),
        mt_expr(":", "conditional-probability-consistency",
                mt_expr("->", mt_keep(number), mt_keep(number), mt_keep(number), "Bool")),
        mt_expr("=", T_CONSISTENT(mt_keep(a), mt_keep(b), mt_keep(ab)),
                CONSISTENT(T_AND, T_LT, T_LE, T_SMALLEST, T_LARGEST, mt_keep(a), mt_keep(b), mt_keep(ab))),
        mt_expr("=",
                mt_expr("Truth_Deduction", T_STV(mt_var("Ps"), mt_var("Pc")), T_STV(mt_var("Qs"), mt_var("Qc")),
                        T_STV(mt_var("Rs"), mt_var("Rc")), T_STV(mt_var("PQs"), mt_var("PQc")), T_STV(mt_var("QRs"), mt_var("QRc"))),
                DEDUCTION(T_IF, T_AND, T_LT, T_ADD, T_SUB, T_MUL, T_DIV, T_MIN, T_STV, T_CONSISTENT, mt_var("Ps"), mt_var("Pc"),
                          mt_var("Qs"), mt_var("Qc"), mt_var("Rs"), mt_var("Rc"), mt_var("PQs"), mt_var("PQc"), mt_var("QRs"),
                          mt_var("QRc"))),
    };
    for (size_t i = 0; i < sizeof definitions / sizeof *definitions; i++) require("a definition", mt_add(m, definitions[i]));
    mt_drop(number), mt_drop(a), mt_drop(b), mt_drop(ab);
}

/* ---- lib_pln's formulas, where every division is /safe ---- */

/* (/safe a b): the quotient where the denominator is positive, else none. */
static inline bool safe_div(double a, double b, double *q)
{
    if (!(b > 0.0)) return false;
    *q = a / b;
    return true;
}
static inline double negate(double x) { return 1.0 - x; }
static inline bool invert(double x, double *q) { return safe_div(1.0, x, q); }
static inline bool pln_w2c(double w, double *c) { return safe_div(w, w + 1, c); }
static inline bool pln_c2w(double c, double *w) { return safe_div(c, 1 - c, w); }

static inline truth pln_negation(truth t) { return (truth){ 1.0 - t.f, t.c }; }

static inline truth modus_ponens(truth a, truth ab) { return (truth){ a.f * ab.f + 0.02 * (1 - a.f), a.c * ab.c }; }

/* NARS's revision with PLN's ceiling of 1.0 rather than 0.99. */
static inline bool pln_revision(truth a, truth b, truth *out)
{
    double w1, w2, f, c;
    if (!pln_c2w(a.c, &w1) || !pln_c2w(b.c, &w2)) return false;
    double w = w1 + w2;
    if (!safe_div(w1 * a.f + w2 * b.f, w, &f) || !pln_w2c(w, &c)) return false;
    *out = (truth){ C_MIN(1.0, f), C_MIN(1.0, C_MAX(C_MAX(c, a.c), b.c)) };
    return true;
}

/* The link types symmetric modus ponens may fire on, one guard equation
   each; any other type has no equation, which prunes the rule. */
static const char *const symmetric_links[] = { "Similarity", "IntentionalSimilarity", "ExtensionalSimilarity" };

static inline bool symmetric_link(const char *type)
{
    for (size_t i = 0; i < sizeof symmetric_links / sizeof *symmetric_links; i++)
        if (strcmp(symmetric_links[i], type) == 0) return true;
    return false;
}

/* Modus ponens over a symmetric link, whose negative branch is a fixed
   (stv 0.2 1.0) rather than a node probability. */
static inline truth symmetric_modus_ponens(truth a, truth ab)
{
    const truth not_ab = { 0.2, 1.0 };
    return (truth){ a.f * ab.f + (not_ab.f * negate(a.f)) * (1.0 + ab.f), C_MIN(C_MIN(ab.c, not_ab.c), a.c) };
}

static inline truth inversion(truth b, truth ab) { return (truth){ ab.f, b.c * (ab.c * 0.6) }; }

/* The PLN book's sim2inh, except that a link whose strength times
   confidence is above 0.99 keeps its own strength. */
static inline bool equivalence_to_implication(truth a, truth b, truth ab, truth *out)
{
    double s, ratio;
    if (0.99 < ab.f * ab.c)
        s = ab.f;
    else if (!safe_div(b.f, a.f, &ratio) || !safe_div((1.0 + ratio) * ab.f, 1.0 + ab.f, &s))
        return false;
    *out = (truth){ s, ab.c };
    return true;
}

/* One of the transitive similarity strength's two inverted halves. */
static inline bool similarity_half(double sB, double sC, double t, double u, double *half)
{
    double tail;
    return safe_div(sC - sB * u, negate(sB), &tail) && invert(t * u + negate(t) * tail, half);
}

static inline bool transitive_similarity_strength(double sA, double sB, double sC, double sAB, double sBC, double *s)
{
    double t1 = ((1.0 + sB / sA) * sAB) / (1.0 + sAB), t2 = ((1.0 + sC / sB) * sBC) / (1.0 + sBC),
           t3 = ((1.0 + sB / sC) * sBC) / (1.0 + sBC), t4 = ((1.0 + sA / sB) * sAB) / (1.0 + sAB), h1, h2;
    return similarity_half(sB, sC, t1, t2, &h1) && similarity_half(sB, sC, t3, t4, &h2) && invert((h1 + h2) - 1.0, s);
}

static inline bool transitive_similarity(truth a, truth b, truth c, truth ab, truth bc, truth *out)
{
    double s;
    if (!transitive_similarity_strength(a.f, b.f, c.f, ab.f, bc.f, &s)) return false;
    *out = (truth){ s, C_MIN(ab.c, bc.c) };
    return true;
}

/* The deduction strength where the consistency conditions hold, none where
   they do not. */
static inline bool simple_deduction_strength(double sA, double sB, double sC, double sAB, double sBC, double *s)
{
    double tail;
    if (!(consistent(sA, sB, sAB) && consistent(sB, sC, sBC))) return false;
    if (0.99 < sB) return *s = sC, true;
    if (!safe_div((1.0 - sAB) * (sC - sB * sBC), 1.0 - sB, &tail)) return false;
    *s = sAB * sBC + tail;
    return true;
}

/* min over five, for integers and floats alike, as MeTTa's min serves both. */
#define MIN5(a, b, c, d, e) C_MIN(a, C_MIN(b, C_MIN(c, C_MIN(d, e))))

static inline bool evaluation_implication(truth a, truth b, truth c, truth ab, truth ac, truth *out)
{
    double s;
    if (!simple_deduction_strength(b.f, a.f, c.f, ab.f, ac.f, &s)) return false;
    *out = (truth){ s, (0.9 * 0.9) * MIN5(b.c, a.c, c.c, ac.c, 0.9 * ab.c) };
    return true;
}

/* PLN's induction and abduction take the three node truths as well as the
   two links. */
static inline bool pln_induction(truth a, truth b, truth c, truth ba, truth bc, truth *out)
{
    double first, share, rest, conf;
    if (!safe_div((ba.f * bc.f) * b.f, a.f, &first) || !safe_div(ba.f * b.f, a.f, &share) ||
        !safe_div(c.f - b.f * bc.f, 1 - b.f, &rest) || !pln_w2c(C_MIN(ba.c, bc.c), &conf))
        return false;
    *out = (truth){ first + (1 - share) * rest, conf };
    return true;
}

static inline bool pln_abduction(truth a, truth b, truth c, truth ab, truth cb, truth *out)
{
    double first, second, conf;
    (void)a;
    if (!safe_div((ab.f * cb.f) * c.f, b.f, &first) || !safe_div(c.f * ((1 - ab.f) * (1 - cb.f)), 1 - b.f, &second) ||
        !pln_w2c(C_MIN(ab.c, cb.c), &conf))
        return false;
    *out = (truth){ first + second, conf };
    return true;
}
#endif
