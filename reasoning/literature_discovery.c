/* Purpose: combine C cosine matching with tagged symbolic source provenance.
 * Owns resources: releases the grounded similarity matcher after query closure.
 * Decides: synthetic vectors and claims demonstrate inference, not treatment advice.
 * Guarantees: a vocabulary gap needs similarity and two paths retain citations
 *   [tested: make check; commit=WORKTREE].
 * Open Obligations: None.
 */
#include "common.h"
#include <math.h>
static mt_status similar(mt_call *call, void *user)
{
    (void)user; const mt_atom *other = mt_arg(call, 0); const char *term = mt_name(other);
    if (!term) return MT_FAIL;
    double x, y;
    if (strcmp(term, "omega-3") == 0) { x = 0.90; y = 0.10; }
    else if (strcmp(term, "aspirin") == 0) { x = 0.10; y = 0.90; }
    else return MT_FAIL;
    double cosine = (0.88*x + 0.16*y) / (hypot(0.88,0.16)*hypot(x,y));
    return cosine >= 0.95 ? mt_answer(call, mt_keep(other)) : MT_FAIL;
}
int main(void)
{
    metta *m = open_engine();
    check("tagged claims", mt_do(m,
      "(fact p1 (reports omega-3 lowers viscosity)) (fact p2 (reports viscosity aggravates raynaud)) "
      "(fact p4 (reports omega-3 lowers platelets)) (fact p5 (reports platelets aggravates raynaud)) "
      "(fact p3 (reports aspirin lowers inflammation)) "
      "(rule abc (suggests $substance $condition) (premises (reports $substance lowers $factor) (reports $factor aggravates $condition)))"));
    check_answers("literal vocabulary gap", mt_run(m, "!(match-under &self prov (suggests fish-oil raynaud))"), "");
    mt_atom *matcher = mt_matcher(similar, NULL, NULL); check("cosine matcher", matcher != NULL);
    mt_atom *query = mt_expr("let", mt_parse("($claim $proof)"),
       mt_parse("(match-under &self prov (suggests $substance raynaud))"),
       mt_expr("unify", mt_keep(matcher), mt_var("substance"),
         mt_expr("Evidence", mt_var("claim"), mt_var("proof")), "Empty"));
    mt_atom *answer = mt_one(mt_eval(m, query));
    check("one inferred hypothesis", answer != NULL);
    check_atom("similarity bridges the vocabulary", mt_at(answer, 1), "(suggests omega-3 raynaud)");
    const mt_atom *proof = mt_at(answer, 2);
    check("alternative proofs", mt_len(proof) == 3); check_atom("provenance sum", mt_at(proof, 0), "plus");
    mt_atom *first = mt_parse("(times (times abc p1) p2)");
    mt_atom *second = mt_parse("(times (times abc p4) p5)");
    check("both independent citation paths", (mt_eq(mt_at(proof, 1), first) && mt_eq(mt_at(proof, 2), second)) ||
        (mt_eq(mt_at(proof, 2), first) && mt_eq(mt_at(proof, 1), second)));
    mt_drop(first); mt_drop(second); mt_drop(answer);
    check_answers("count independent paths", mt_run(m,
        "!(match-under &self counting (suggests omega-3 raynaud))"), "((suggests omega-3 raynaud) 2)");
    check("release matcher", mt_object_free(matcher));
    return done(m, "literature_discovery");
}
