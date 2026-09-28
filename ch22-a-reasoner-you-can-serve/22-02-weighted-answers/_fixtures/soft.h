/* Purpose: C's model of lib_soft's weak unification [source
 *   2026-09-29T04:48:10+10:00: lib/lib_soft/lib.metta, sym-sim to soft-best,
 *   and engine/metta/space_hooks.pl, is-symbol]:
 *   - two symbols are 1.0 close when they are one symbol, and otherwise as
 *     close as the closest declared similarity between them, read both
 *     ways, or 0.0;
 *   - a symbol is a symbol by how it is written, whatever the engine holds
 *     for the name, which is what keeps min a symbol;
 *   - a pattern variable matches anything at 1.0 and binds to it, so a
 *     variable met twice must meet the same atom twice or the score has no
 *     answer; two expressions of one length compare position by position,
 *     the head included, under the aggregation; two symbols compare by
 *     closeness; anything else is 1.0 when identical and 0.0 when not;
 *   - min starts from 1.0, takes minima and stops at 0.0, since no later
 *     position can raise it; mean starts from 0.0, sums every position and
 *     divides by their count;
 *   - a space's aggregation is the one it declares, or min.
 *   The model is the similarity facts and the declared aggregation, the
 *   state lib_soft reads out of the space; a twin changes it as it changes
 *   the space.
 *   soft_match() answers as lib_soft does, as the (score candidate) pairs
 *   of lib_measure, whose model is measure.h.
 * Assumes: the includer defines MT_SHORTHAND first; every candidate is ground,
 *   so a pattern variable meeting an atom a second time asks whether the two
 *   are identical where the library's let unifies them.
 * Guarantees: each function answers what lib_soft answers for the same
 *   operands in a space holding the same facts, bindings included
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#ifndef CH22_SOFT_H
#define CH22_SOFT_H
#include <cmetta.h>
#include <stdlib.h>
#include <string.h>
#include "measure.h"

/* Each MeTTa operator twice, for a body written once over its operators:
   C_X computes in C, and T_X builds the atom (X a b). */
#define C_MIN(a, b) ((a) < (b) ? (a) : (b))

typedef enum aggregation { SOFT_MIN, SOFT_MEAN } aggregation;
static const char *const aggregation_names[] = { [SOFT_MIN] = "min", [SOFT_MEAN] = "mean" };

typedef struct similarity {
    const char *a, *b;
    double degree;
} similarity;

typedef struct soft {
    const similarity *facts;
    size_t n;
    aggregation declared;
} soft;

/* The pattern variables one score has bound so far, each to the atom it
   met, both borrowed from the pattern and the candidate. */
typedef struct soft_binding {
    const mt_atom *var, *value;
} soft_binding;

typedef struct soft_bindings {
    soft_binding *at;
    size_t n;
} soft_bindings;

static inline void soft_bindings_free(soft_bindings *b)
{
    free(b->at);
    *b = (soft_bindings){ NULL, 0 };
}

/* The atom VAR is bound to, or NULL. */
static inline const mt_atom *soft_bound(const soft_bindings *b, const mt_atom *var)
{
    for (size_t i = 0; i < b->n; i++)
        if (mt_eq(b->at[i].var, var)) return b->at[i].value;
    return NULL;
}

static inline bool soft_symbol(const mt_atom *x) { return mt_kind_of(x) == MT_SYMBOL; }

static inline double sym_sim(const soft *s, const mt_atom *a, const mt_atom *b)
{
    if (mt_eq(a, b)) return 1.0;
    double closest = 0.0;
    for (size_t i = 0; i < s->n; i++) {
        bool forward = strcmp(s->facts[i].a, mt_name(a)) == 0 && strcmp(s->facts[i].b, mt_name(b)) == 0,
             backward = strcmp(s->facts[i].a, mt_name(b)) == 0 && strcmp(s->facts[i].b, mt_name(a)) == 0;
        if ((forward || backward) && s->facts[i].degree > closest) closest = s->facts[i].degree;
    }
    return closest;
}

static inline bool soft_score_by(const soft *s, aggregation agg, const mt_atom *p, const mt_atom *a, soft_bindings *bound,
                                 double *score);

/* The positions of P and A under AGG from ACC, the walk soft-walk runs. */
static inline bool soft_walk(const soft *s, aggregation agg, const mt_atom *p, const mt_atom *a, double acc, soft_bindings *bound,
                             double *out)
{
    for (size_t i = 0; i < mt_len(p) && !(agg == SOFT_MIN && acc == 0.0); i++) {
        double position;
        if (!soft_score_by(s, agg, mt_at(p, i), mt_at(a, i), bound, &position)) return false;
        acc = agg == SOFT_MIN ? C_MIN(acc, position) : acc + position;
    }
    *out = acc;
    return true;
}

/* soft-fold: the walk from its aggregation's start, a mean divided by the
   count of positions. */
static inline bool soft_fold(const soft *s, aggregation agg, const mt_atom *p, const mt_atom *a, soft_bindings *bound, double *out)
{
    if (!soft_walk(s, agg, p, a, agg == SOFT_MIN ? 1.0 : 0.0, bound, out)) return false;
    if (agg == SOFT_MEAN) *out = *out / (double)mt_len(p);
    return true;
}

/* soft-score-by: false where a variable met twice meets two atoms, which
   the library's let cannot unify, so the score has no answer. */
static inline bool soft_score_by(const soft *s, aggregation agg, const mt_atom *p, const mt_atom *a, soft_bindings *bound,
                                 double *score)
{
    if (mt_kind_of(p) == MT_VARIABLE) {
        const mt_atom *earlier = soft_bound(bound, p);
        if (earlier) return mt_eq(earlier, a) && (*score = 1.0, true);
        soft_binding *grown = realloc(bound->at, (bound->n + 1) * sizeof *grown);
        require("room", grown != NULL);
        bound->at = grown;
        bound->at[bound->n++] = (soft_binding){ p, a };
        return *score = 1.0, true;
    }
    if (mt_kind_of(p) == MT_EXPR && mt_kind_of(a) == MT_EXPR)
        return mt_len(p) == mt_len(a) ? soft_fold(s, agg, p, a, bound, score) : (*score = 0.0, true);
    *score = soft_symbol(p) && soft_symbol(a) ? sym_sim(s, p, a) : mt_eq(p, a) ? 1.0 : 0.0;
    return true;
}

/* soft-score under the space's own aggregation. */
static inline bool soft_score(const soft *s, const mt_atom *p, const mt_atom *a, soft_bindings *bound, double *score)
{
    return soft_score_by(s, s->declared, p, a, bound, score);
}

/* soft-match: each of a space's atoms in order, scored against PATTERN
   under the space's aggregation, each score a fresh set of bindings, kept at
   LEAST or above as a (score candidate) pair. */
static inline ws soft_match(const soft *s, mt_atom *const *atoms, size_t n, const mt_atom *pattern, double least)
{
    ws matched = { NULL, 0 };
    for (size_t i = 0; i < n; i++) {
        soft_bindings bound = { NULL, 0 };
        double score;
        if (soft_score(s, pattern, atoms[i], &bound, &score) && score >= least) ws_push(&matched, score, mt_keep(atoms[i]));
        soft_bindings_free(&bound);
    }
    return matched;
}
#endif
