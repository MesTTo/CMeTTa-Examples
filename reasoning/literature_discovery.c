/* Purpose: symbols and vectors in one inference. Findings are tagged facts
 *   and a two-step rule suggests a treatment through a shared factor; the
 *   query names a substance the literature never mentions, and a C matcher
 *   bridges the vocabulary gap by cosine similarity, while the prov algebra
 *   keeps both citation paths behind the one hypothesis.
 * Decides: the vectors and findings are synthetic; they demonstrate the
 *   inference and advise nothing.
 * Owns resources: the matcher value, released after the query closes.
 * Guarantees: the literal query finds nothing, the similarity-bridged one
 *   finds one hypothesis with two independent proofs [tested: make check;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include <math.h>

/* Candidates for the matcher's other operand: a term is similar to the
   query's fish oil when their embeddings' cosine is at least 0.95. */
static mt_status similar(mt_call *call, void *user)
{
    (void)user;
    static const struct { const char *term; double x, y; } embeddings[] = {
        { "omega-3", 0.90, 0.10 }, { "aspirin", 0.10, 0.90 },
    };
    static const double fish_oil[2] = { 0.88, 0.16 };
    const char *term = mt_name(mt_arg(call, 0));
    if (!term) return MT_FAIL;
    for (size_t i = 0; i < 2; i++)
        if (strcmp(term, embeddings[i].term) == 0) {
            double cosine = (fish_oil[0] * embeddings[i].x + fish_oil[1] * embeddings[i].y) /
                            (hypot(fish_oil[0], fish_oil[1]) * hypot(embeddings[i].x, embeddings[i].y));
            return cosine >= 0.95 ? mt_answer(call, mt_keep(mt_arg(call, 0))) : MT_FAIL;
        }
    return MT_FAIL;
}

int main(void)
{
    metta *m = open_engine();
    static const struct { const char *paper, *subject, *verb, *object; } findings[] = {
        { "p1", "omega-3", "lowers", "viscosity" },   { "p2", "viscosity", "aggravates", "raynaud" },
        { "p4", "omega-3", "lowers", "platelets" },   { "p5", "platelets", "aggravates", "raynaud" },
        { "p3", "aspirin", "lowers", "inflammation" },
    };
    for (size_t i = 0; i < 5; i++)
        require("a finding", mt_add(m, E("fact", findings[i].paper,
            E("reports", findings[i].subject, findings[i].verb, findings[i].object))));
    /* (rule abc (suggests $s $c) (premises (reports $s lowers $f) (reports $f aggravates $c))) */
    require("the ABC rule", mt_add(m, E("rule", "abc", E("suggests", V("s"), V("c")),
        E("premises", E("reports", V("s"), "lowers", V("f")),
                      E("reports", V("f"), "aggravates", V("c"))))));

    check_none("the literature never names fish oil",
               mt_eval(m, E("match-under", "&self", "prov", E("suggests", "fish-oil", "raynaud"))));

    mt_atom *fish_oil = mt_matcher(similar, NULL, NULL);
    require("build the similarity matcher", fish_oil != NULL);
    /* (let ($claim $proof) (match-under &self prov (suggests $s raynaud))
            (unify <fish-oil> $s (Evidence $claim $proof) Empty)) */
    mt_atom *hypothesis = mt_one(mt_eval(m, E("let", E(V("claim"), V("proof")),
        E("match-under", "&self", "prov", E("suggests", V("s"), "raynaud")),
        E("unify", mt_keep(fish_oil), V("s"), E("Evidence", V("claim"), V("proof")), "Empty"))));
    require("one hypothesis", hypothesis != NULL);
    check("similarity bridges the vocabulary",
          alpha_equal(mt_at(hypothesis, 1), E("suggests", "omega-3", "raynaud")));
    const mt_atom *proof = mt_at(hypothesis, 2);
    mt_atom *first = E("times", E("times", "abc", "p1"), "p2");
    mt_atom *second = E("times", E("times", "abc", "p4"), "p5");
    check("the proof is a sum of two", mt_len(proof) == 3 && alpha_equal(mt_at(proof, 0), S("plus")));
    check("of both independent citation paths",
          (mt_alpha_eq(mt_at(proof, 1), first) && mt_alpha_eq(mt_at(proof, 2), second)) ||
          (mt_alpha_eq(mt_at(proof, 2), first) && mt_alpha_eq(mt_at(proof, 1), second)));
    mt_drop(first);
    mt_drop(second);
    mt_drop(hypothesis);

    check_answers("counting the same derivations says two",
                  mt_eval(m, E("match-under", "&self", "counting", E("suggests", "omega-3", "raynaud"))),
                  E(E("suggests", "omega-3", "raynaud"), 2));
    require("release the matcher", mt_object_free(fish_oil));
    return done(m);
}
