/* Purpose: PLN's derivation loop and the priority queue under it, held to
 *   C's model of them in derive.h, the same model NARS's loop is held to,
 *   since lib_pln writes the same five ideas over its own tuple helpers. The
 *   configuration numbers are C's table. The rules the loop runs are C's
 *   model of lib_pln's over plain terms and links between them: revision
 *   where the two terms are one, modus ponens along an Implication, and
 *   symmetric modus ponens along a link its guard admits, each with its
 *   truth from pln.h; alone, a Not gives its negation. Every other rule of
 *   lib_pln reads a node's truth through STV, which lib_pln declares and
 *   leaves empty, or needs an Evaluation, so it derives nothing here. C runs
 *   its loop over the original's premises and holds the engine's queues and
 *   query answer to what the loop leaves.
 * Guarantees: all twenty-seven claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <string.h>
#include "_fixtures/nars_truth.h"
#include "_fixtures/pln.h"
#include "_fixtures/derive.h"

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

static const struct {
    const char *name;
    int64_t value;
} config[] = { { "PLN.Config.MaxSteps", 100 }, { "PLN.Config.TaskQueueSize", 10 }, { "PLN.Config.BeliefQueueSize", 100 } };
enum { MAX_STEPS, TASK_QUEUE_SIZE, BELIEF_QUEUE_SIZE };

/* (Type a b) as its type and ends, or false for any other term. */
static bool link_of(const mt_atom *term, const char **type, const mt_atom **a, const mt_atom **b)
{
    if (mt_kind_of(term) != MT_EXPR || mt_len(term) != 3 || mt_kind_of(mt_at(term, 0)) != MT_SYMBOL) return false;
    *type = mt_name(mt_at(term, 0)), *a = mt_at(term, 1), *b = mt_at(term, 2);
    return true;
}

/* What (|- x y) answers for two judgements, in lib_pln's rule order. */
static void pln_pair(const mt_atom *x, const mt_atom *y, mt_list *out)
{
    const mt_atom *tx = mt_at(x, 0), *ty = mt_at(y, 0), *a, *b;
    const char *type;
    truth vx = truth_of(x), vy = truth_of(y), revised;
    if (mt_eq(tx, ty) && pln_revision(vx, vy, &revised)) push(out, E(mt_keep(tx), stv(revised)));
    if (!link_of(ty, &type, &a, &b) || !mt_eq(tx, a)) return;
    if (strcmp(type, "Implication") == 0) push(out, E(mt_keep(b), stv(modus_ponens(vx, vy))));
    if (symmetric_link(type)) push(out, E(mt_keep(b), stv(symmetric_modus_ponens(vx, vy))));
}

/* What (|- x) answers: a Not eliminated. */
static void pln_alone(const mt_atom *x, mt_list *out)
{
    const mt_atom *tx = mt_at(x, 0);
    if (mt_kind_of(tx) == MT_EXPR && mt_len(tx) == 2 && mt_kind_of(mt_at(tx, 0)) == MT_SYMBOL && strcmp(mt_name(mt_at(tx, 0)), "Not") == 0)
        push(out, E(mt_keep(mt_at(tx, 1)), stv(pln_negation(truth_of(x)))));
}

static const rules pln = { pln_pair, pln_alone };

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("lib_pln", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_pln")))));

    for (size_t i = 0; i < sizeof config / sizeof *config; i++) assert(answers_are(mt_eval(m, E(config[i].name)), E(config[i].value)) && config[i].name);

    mt_atom *disjoint[][2] = { { E(1, 2), E(3, 4) }, { E(1, 2), E(2, 3) }, { mt_unit(), E(1) } };
    for (size_t i = 0; i < sizeof disjoint / sizeof *disjoint; i++) {
        assert(answers_are(mt_eval(m, E("StampDisjoint", mt_keep(disjoint[i][0]), mt_keep(disjoint[i][1]))), E(B(stamps_disjoint(disjoint[i][0], disjoint[i][1]))))
               && "StampDisjoint");
        mt_drop(disjoint[i][0]), mt_drop(disjoint[i][1]);
    }
    mt_atom *concat[][2] = { { E(3, 1), E(2) }, { E(3, 1), mt_unit() } };
    for (size_t i = 0; i < 2; i++) {
        assert(answers_are(mt_eval(m, E("StampConcat", mt_keep(concat[i][0]), mt_keep(concat[i][1]))), E(stamp_concat(concat[i][0], concat[i][1])))
               && "StampConcat");
        mt_drop(concat[i][0]), mt_drop(concat[i][1]);
    }
    assert(atom_is(mt_one(mt_eval(m, E("StampConcat", E(1), E(2)))), mt_one(mt_eval(m, E("StampConcat", E(2), E(1)))))
           && "StampConcat is order-free");

    mt_atom *quiet = sentence(S("a"), (truth){ 1.0, 0.5 }, E(1)), *loud = sentence(S("b"), (truth){ 1.0, 0.9 }, E(2));
    mt_atom *certain_a = sentence(S("a"), (truth){ 1.0, 0.9 }, E(1));
    assert(answers_are(mt_eval(m, E("PriorityRank", mt_keep(certain_a))), E(priority(certain_a))) && "PriorityRank");
    assert(answers_are(mt_eval(m, E("PriorityRank", mt_unit())), E(priority(NULL))) && "PriorityRank of ()");
    assert(answers_are(mt_eval(m, E("PriorityRankNeg", mt_keep(certain_a))), E(priority_neg(certain_a))) && "PriorityRankNeg");
    assert(answers_are(mt_eval(m, E("PriorityRankNeg", mt_unit())), E(priority_neg(NULL))) && "PriorityRankNeg of ()");
    mt_atom *answer = E(stv((truth){ 1.0, 0.9 }), E(1));
    assert(answers_are(mt_eval(m, E("ConfidenceRank", mt_keep(answer))), E(confidence(answer))) && "ConfidenceRank");
    assert(answers_are(mt_eval(m, E("ConfidenceRank", mt_unit())), E((int64_t)NO_CONFIDENCE)) && "ConfidenceRank of ()");
    mt_drop(answer), mt_drop(certain_a);

    mt_list pair = QUEUE(mt_keep(quiet), mt_keep(loud)), none = { NULL, 0 };
    assert(answers_are(mt_eval(m, E("BestCandidate", "PriorityRank", mt_unit(), tuple(&pair))), E(candidate(best_candidate(priority, &pair))))
           && "the most confident");
    assert(answers_are(mt_eval(m, E("BestCandidate", "PriorityRankNeg", mt_unit(), tuple(&pair))), E(candidate(best_candidate(priority_neg, &pair))))
           && "the least confident");
    assert(answers_are(mt_eval(m, E("BestCandidate", "PriorityRank", mt_unit(), mt_unit())), E(candidate(best_candidate(priority, &none))))
           && "nothing to choose from");

    mt_list single = QUEUE(mt_keep(quiet));
    assert(answers_are(mt_eval(m, E("LimitSize", tuple(&single), 5)), E(limited(&single, 5))) && "LimitSize under the bound");
    assert(answers_are(mt_eval(m, E("LimitSize", tuple(&pair), 2)), E(limited(&pair, 2))) && "LimitSize at two");
    assert(answers_are(mt_eval(m, E("LimitSize", tuple(&pair), 1)), E(limited(&pair, 1))) && "LimitSize at one");
    /* At a size of 0 no count passes the bound, and the empty queue is its
       own limit. */
    assert(answers_are(mt_eval(m, E("LimitSize", tuple(&none), 0)), E(limited(&none, 0))) && "LimitSize at zero answers the empty queue");
    mt_list_free(single), mt_list_free(pair), mt_drop(quiet), mt_drop(loud);

    /* An implication and a fact, two steps: modus ponens gives b with both
       IDs, and the loop ends with the queues C's loop ends with. */
    mt_atom *rule = sentence(E("Implication", "a", "b"), (truth){ 1.0, 0.9 }, E(1)), *fact = sentence(S("a"), (truth){ 1.0, 0.9 }, E(2));
    const bounds configured = { (size_t)config[TASK_QUEUE_SIZE].value, (size_t)config[BELIEF_QUEUE_SIZE].value };
    mt_list tasks = QUEUE(mt_keep(rule)), beliefs = QUEUE(mt_keep(fact));
    mt_atom *queues = engine_derive(m, "PLN.Derive", &tasks, &beliefs, 2, NULL);
    derive(&pln, &tasks, &beliefs, 1, 2, configured);
    assert(atom_is(mt_keep(mt_at(queues, 1)), tuple(&beliefs)) && "modus ponens joins the beliefs");
    mt_list_free(tasks), mt_list_free(beliefs), mt_drop(queues);

    tasks = QUEUE(mt_keep(rule)), beliefs = QUEUE(mt_keep(fact));
    queues = engine_derive(m, "PLN.Derive", &tasks, &beliefs, 0, NULL);
    derive(&pln, &tasks, &beliefs, 1, 0, configured);
    assert(atom_is(queues, E(tuple(&tasks), tuple(&beliefs))) && "a budget of zero keeps both queues");
    mt_list_free(tasks), mt_list_free(beliefs);

    tasks = (mt_list){ NULL, 0 }, beliefs = QUEUE(mt_keep(fact));
    queues = engine_derive(m, "PLN.Derive", &tasks, &beliefs, config[MAX_STEPS].value, NULL);
    derive(&pln, &tasks, &beliefs, 1, config[MAX_STEPS].value, configured);
    assert(atom_is(queues, E(tuple(&tasks), tuple(&beliefs))) && "no task stops the loop");
    mt_list_free(tasks), mt_list_free(beliefs);

    /* Queues bounded at 0 keep nothing: the first selection derives, both
       queues are cut to (), and the empty task queue stops the loop. */
    const bounds nothing = { 0, 0 };
    tasks = QUEUE(mt_keep(rule)), beliefs = QUEUE(mt_keep(fact));
    queues = engine_derive(m, "PLN.Derive", &tasks, &beliefs, config[MAX_STEPS].value, &nothing);
    derive(&pln, &tasks, &beliefs, 1, config[MAX_STEPS].value, nothing);
    assert(atom_is(queues, E(tuple(&tasks), tuple(&beliefs))) && "queues bounded at zero keep nothing");
    mt_list_free(tasks), mt_list_free(beliefs);

    /* The query: the loop over the knowledge base as both queues, then the
       most confident belief about b. */
    mt_list kb = QUEUE(mt_keep(rule), mt_keep(fact));
    tasks = QUEUE(mt_keep(rule), mt_keep(fact)), beliefs = QUEUE(mt_keep(rule), mt_keep(fact));
    derive(&pln, &tasks, &beliefs, 1, 2, configured);
    mt_atom *b = S("b");
    assert(answers_are(mt_eval(m, E("PLN.Query", tuple(&kb), mt_keep(b), 2)), E(query(&beliefs, b))) && "PLN.Query");
    mt_list_free(kb), mt_list_free(tasks), mt_list_free(beliefs), mt_drop(b), mt_drop(rule), mt_drop(fact);
    mt_close(m);
    return 0;
}
