/* Purpose: symbols and vectors in one inference. Findings are tagged facts
 *   and a two-step rule suggests a treatment through a shared factor; the
 *   query names a substance the literature never mentions, and a C matcher
 *   bridges the vocabulary gap by cosine similarity, while the prov algebra
 *   keeps both citation paths behind the one hypothesis.
 * Decides: the vectors and findings are synthetic; they demonstrate the
 *   inference and advise nothing.
 * Owns resources: the matcher value, released after the query closes.
 * Guarantees: the literal query finds nothing, the similarity-bridged one
 *   finds one hypothesis with two independent proofs
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 * Build: cc literature_discovery.c $(pkg-config --cflags --libs cmetta) -lm
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

/* The same question as a truth value, for a condition with more in it:
   borrows the atom the program holds and takes the expectation. */
static inline bool alpha_equal(const mt_atom *got, mt_atom *want)
{
    bool equal = got && want && mt_alpha_eq(got, want);
    mt_drop(want);
    return equal;
}

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
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
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

    assert(!mt_first(mt_eval(m, E("match-under", "&self", "prov", E("suggests", "fish-oil", "raynaud")))) && mt_ok()
           && "the literature never names fish oil");

    mt_atom *fish_oil = mt_matcher(similar, NULL, NULL);
    require("build the similarity matcher", fish_oil != NULL);
    /* (let ($claim $proof) (match-under &self prov (suggests $s raynaud))
            (unify <fish-oil> $s (Evidence $claim $proof) Empty)) */
    mt_atom *hypothesis = mt_one(mt_eval(m, E("let", E(V("claim"), V("proof")),
        E("match-under", "&self", "prov", E("suggests", V("s"), "raynaud")),
        E("unify", mt_keep(fish_oil), V("s"), E("Evidence", V("claim"), V("proof")), "Empty"))));
    require("one hypothesis", hypothesis != NULL);
    assert(alpha_equal(mt_at(hypothesis, 1), E("suggests", "omega-3", "raynaud"))
           && "similarity bridges the vocabulary");
    const mt_atom *proof = mt_at(hypothesis, 2);
    mt_atom *first = E("times", E("times", "abc", "p1"), "p2");
    mt_atom *second = E("times", E("times", "abc", "p4"), "p5");
    assert(mt_len(proof) == 3 && alpha_equal(mt_at(proof, 0), S("plus")) && "the proof is a sum of two");
    assert(((mt_alpha_eq(mt_at(proof, 1), first) && mt_alpha_eq(mt_at(proof, 2), second)) ||
            (mt_alpha_eq(mt_at(proof, 2), first) && mt_alpha_eq(mt_at(proof, 1), second)))
           && "of both independent citation paths");
    mt_drop(first);
    mt_drop(second);
    mt_drop(hypothesis);

    assert(answers_are(mt_eval(m, E("match-under", "&self", "counting", E("suggests", "omega-3", "raynaud"))), E(E(E("suggests", "omega-3", "raynaud"), 2)))
           && "counting the same derivations says two");
    require("release the matcher", mt_object_free(fish_oil));
    mt_close(m);
    return 0;
}
