/* Purpose: the measure a cost-ordered translator rule is applied by, as C
 *   computes it. A form's cost folds over its nodes: a symbol whose rule
 *   declared a cost weighs that, and every other node weighs one, a variable
 *   and the empty expression included. A rule read both ways rewrites a call
 *   only when what its equation produces costs strictly less than the call,
 *   which is what stops a bidirectional rule and its inverse rewriting each
 *   other forever [source: engine/translator_rules.pl,
 *   translator_form_cost/2 and translator_rule_orients/4;
 *   commit=214188f1d5b5018a0061ea1bc72b104e69137b8f].
 * Guarantees: the cost and the orientation the engine computes for the same
 *   form and the same declarations [tested 2026-09-27T00:35:58+10:00:
 *   make -C extensions/cmetta corpus-check].
 */
#ifndef CH20_COSTS_H
#define CH20_COSTS_H
#include <cmetta.h>
#include <string.h>

/* A cost a rule declared for its head. */
typedef struct declared_cost {
    const char *head;
    int64_t cost;
} declared_cost;

/* Time: Theta(n * d) comparisons for n nodes and d declared costs. */
static inline int64_t form_cost(const mt_atom *form, const declared_cost *declared, size_t d)
{
    if (mt_kind_of(form) == MT_EXPR) {
        if (mt_len(form) == 0) return 1;
        int64_t sum = 0;
        for (size_t i = 0; i < mt_len(form); i++) sum += form_cost(mt_at(form, i), declared, d);
        return sum;
    }
    if (mt_kind_of(form) == MT_SYMBOL)
        for (size_t i = 0; i < d; i++)
            if (strcmp(mt_name(form), declared[i].head) == 0) return declared[i].cost;
    return 1;
}

/* What a cost-ordered rule answers for CALL, given what its equation
   PRODUCES: the product when it costs strictly less, else the call as it was
   written. TAKES both, answers one and drops the other. */
static inline mt_atom *oriented(mt_atom *call, mt_atom *produced, const declared_cost *declared, size_t d)
{
    bool lower = form_cost(produced, declared, d) < form_cost(call, declared, d);
    mt_drop(lower ? call : produced);
    return lower ? produced : call;
}
#endif
