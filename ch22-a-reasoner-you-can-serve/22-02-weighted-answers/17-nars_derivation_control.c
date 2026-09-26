/* Purpose: what NARS does between inferences, held to C's model of it in
 *   derive.h: the three configuration numbers are C's table, and the stamp
 *   rule, the ranks, the bounded queue and the loop are derive.h's
 *   functions, so each claim asks lib_nars for what C has already computed.
 *   The rules the loop runs are C's too, lib_nars's over inheritance between
 *   plain terms: revision where the two terms are one, then NAL-1's four
 *   syllogisms in the library's order, deduction, induction, abduction and
 *   exemplification, each with its truth function from nars_truth.h. Every
 *   other rule of lib_nars needs a compound term, a set, an intersection, a
 *   difference, a product, a negation, a conjunction or an implication, and
 *   none of these premises has one. C runs its loop over the original's two
 *   premises and holds the engine's beliefs to hold the sentence C's loop
 *   derives about a --> c.
 * Guarantees: all twenty-eight claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <string.h>
#include "_fixtures/nars_truth.h"
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
} config[] = { { "NARS.Config.MaxSteps", 100 }, { "NARS.Config.TaskQueueSize", 10 }, { "NARS.Config.BeliefQueueSize", 100 } };
enum { MAX_STEPS, TASK_QUEUE_SIZE, BELIEF_QUEUE_SIZE };

/* (--> s p) as its subject and predicate, or false for any other term. */
static bool inheritance(const mt_atom *term, const mt_atom **s, const mt_atom **p)
{
    if (mt_kind_of(term) != MT_EXPR || mt_len(term) != 3 || mt_kind_of(mt_at(term, 0)) != MT_SYMBOL ||
        strcmp(mt_name(mt_at(term, 0)), "-->") != 0)
        return false;
    *s = mt_at(term, 1), *p = mt_at(term, 2);
    return true;
}

static mt_atom *arrow(const mt_atom *s, const mt_atom *p) { return E("-->", mt_keep(s), mt_keep(p)); }

/* What (|- x y) answers for two judgements about inheritance. */
static void nal1(const mt_atom *x, const mt_atom *y, mt_list *out)
{
    const mt_atom *tx = mt_at(x, 0), *ty = mt_at(y, 0), *xs, *xp, *ys, *yp;
    truth vx = truth_of(x), vy = truth_of(y);
    if (mt_eq(tx, ty)) push(out, E(mt_keep(tx), stv(revision(vx, vy))));
    if (!inheritance(tx, &xs, &xp) || !inheritance(ty, &ys, &yp)) return;
    if (mt_eq(xp, ys)) push(out, E(arrow(xs, yp), stv(deduction(vx, vy))));
    if (mt_eq(xs, ys)) push(out, E(arrow(yp, xp), stv(induction(vx, vy))));
    if (mt_eq(xp, yp)) push(out, E(arrow(ys, xs), stv(abduction(vx, vy))));
    if (mt_eq(xp, ys)) push(out, E(arrow(yp, xs), stv(exemplification(vx, vy))));
}

static const rules nars = { nal1, NULL };

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("lib_nars", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_nars")))));

    for (size_t i = 0; i < sizeof config / sizeof *config; i++) assert(answers_are(mt_eval(m, E(config[i].name)), E(config[i].value)) && config[i].name);

    /* Evidence counted once: stamps combine only when they share no ID. */
    mt_atom *disjoint[][2] = { { E(1, 2), E(3, 4) }, { E(1, 2), E(2, 3) }, { mt_unit(), mt_unit() }, { E(1), mt_unit() } };
    for (size_t i = 0; i < sizeof disjoint / sizeof *disjoint; i++) {
        assert(answers_are(mt_eval(m, E("StampDisjoint", mt_keep(disjoint[i][0]), mt_keep(disjoint[i][1]))), E(B(stamps_disjoint(disjoint[i][0], disjoint[i][1]))))
               && "StampDisjoint");
        mt_drop(disjoint[i][0]), mt_drop(disjoint[i][1]);
    }

    /* A merge sorted and order-free, where an empty addition is the identity
       and leaves the stamp unsorted. */
    mt_atom *concat[][2] = { { E(3, 1), E(2) }, { E(3, 1), mt_unit() } };
    for (size_t i = 0; i < 2; i++) {
        assert(answers_are(mt_eval(m, E("StampConcat", mt_keep(concat[i][0]), mt_keep(concat[i][1]))), E(stamp_concat(concat[i][0], concat[i][1])))
               && "StampConcat");
        mt_drop(concat[i][0]), mt_drop(concat[i][1]);
    }
    assert(atom_is(mt_one(mt_eval(m, E("StampConcat", E(1), E(2)))), mt_one(mt_eval(m, E("StampConcat", E(2), E(1)))))
           && "StampConcat is order-free");

    /* The ranks: a sentence's confidence, and the empty tuple's floor. */
    mt_atom *quiet = sentence(S("a"), (truth){ 1.0, 0.5 }, E(1)), *loud = sentence(S("b"), (truth){ 1.0, 0.9 }, E(2));
    assert(answers_are(mt_eval(m, E("PriorityRank", mt_keep(loud))), E(priority(loud))) && "PriorityRank");
    assert(answers_are(mt_eval(m, E("PriorityRank", mt_unit())), E(priority(NULL))) && "PriorityRank of ()");
    assert(answers_are(mt_eval(m, E("PriorityRankNeg", mt_keep(loud))), E(priority_neg(loud))) && "PriorityRankNeg");
    assert(answers_are(mt_eval(m, E("PriorityRankNeg", mt_unit())), E(priority_neg(NULL))) && "PriorityRankNeg of ()");
    mt_atom *answer = E(stv((truth){ 1.0, 0.9 }), E(1));
    assert(answers_are(mt_eval(m, E("ConfidenceRank", mt_keep(answer))), E(confidence(answer))) && "ConfidenceRank");
    assert(answers_are(mt_eval(m, E("ConfidenceRank", mt_unit())), E((int64_t)NO_CONFIDENCE)) && "ConfidenceRank of ()");
    mt_drop(answer);

    /* The queue: one fold, ranked either way. */
    mt_list pair = QUEUE(mt_keep(quiet), mt_keep(loud)), none = { NULL, 0 };
    assert(answers_are(mt_eval(m, E("BestCandidate", "PriorityRank", mt_unit(), tuple(&pair))), E(candidate(best_candidate(priority, &pair))))
           && "the most confident");
    assert(answers_are(mt_eval(m, E("BestCandidate", "PriorityRankNeg", mt_unit(), tuple(&pair))), E(candidate(best_candidate(priority_neg, &pair))))
           && "the least confident");
    assert(answers_are(mt_eval(m, E("BestCandidate", "PriorityRank", mt_unit(), mt_unit())), E(candidate(best_candidate(priority, &none))))
           && "nothing to choose from");

    /* The bound leaves size minus one, the most confident. */
    mt_list single = QUEUE(mt_keep(quiet));
    assert(answers_are(mt_eval(m, E("LimitSize", tuple(&single), 5)), E(limited(&single, 5))) && "LimitSize under the bound");
    assert(answers_are(mt_eval(m, E("LimitSize", tuple(&pair), 2)), E(limited(&pair, 2))) && "LimitSize at two");
    assert(answers_are(mt_eval(m, E("LimitSize", tuple(&pair), 1)), E(limited(&pair, 1))) && "LimitSize at one");
    /* At a size of 0 no length passes the bound, and the empty queue is its
       own limit. */
    assert(answers_are(mt_eval(m, E("LimitSize", tuple(&none), 0)), E(limited(&none, 0))) && "LimitSize at zero answers the empty queue");
    mt_list three = QUEUE(sentence(S("a"), (truth){ 1.0, 0.1 }, E(1)), mt_keep(loud), sentence(S("c"), (truth){ 1.0, 0.5 }, E(3)));
    assert(answers_are(mt_eval(m, E("LimitSize", tuple(&three), 3)), E(limited(&three, 3))) && "LimitSize forgets the least confident");
    mt_list_free(single), mt_list_free(three), mt_list_free(pair), mt_drop(quiet), mt_drop(loud);

    /* The loop over a --> b and b --> c, two steps. */
    mt_atom *premise = sentence(E("-->", "a", "b"), (truth){ 1.0, 0.9 }, E(1)), *belief = sentence(E("-->", "b", "c"), (truth){ 1.0, 0.9 }, E(2));
    const bounds configured = { (size_t)config[TASK_QUEUE_SIZE].value, (size_t)config[BELIEF_QUEUE_SIZE].value };
    mt_list tasks = QUEUE(mt_keep(premise)), beliefs = QUEUE(mt_keep(belief));
    mt_atom *queues = engine_derive(m, "NARS.Derive", &tasks, &beliefs, 2, NULL);
    derive(&nars, &tasks, &beliefs, 1, 2, configured);
    mt_atom *goal = E("-->", "a", "c");
    const mt_atom *derived = NULL;
    for (size_t i = 0; i < beliefs.len && !derived; i++)
        if (mt_eq(mt_at(judgement(beliefs.items[i]), 0), goal)) derived = beliefs.items[i];
    require("C's loop derives a --> c", derived != NULL);
    mt_list engine_beliefs = items_of(mt_at(queues, 1));
    assert(holds(&engine_beliefs, derived) && "the engine's beliefs hold C's a --> c");
    mt_list_free(engine_beliefs), mt_list_free(tasks), mt_list_free(beliefs), mt_drop(queues), mt_drop(goal);

    /* A budget of zero runs no step, and no task stops the loop. */
    tasks = QUEUE(mt_keep(premise)), beliefs = QUEUE(mt_keep(belief));
    queues = engine_derive(m, "NARS.Derive", &tasks, &beliefs, 0, NULL);
    derive(&nars, &tasks, &beliefs, 1, 0, configured);
    assert(atom_is(mt_keep(mt_at(queues, 1)), tuple(&beliefs)) && "a budget of zero keeps the beliefs");
    mt_list_free(tasks), mt_list_free(beliefs), mt_drop(queues);

    tasks = (mt_list){ NULL, 0 }, beliefs = QUEUE(mt_keep(belief));
    queues = engine_derive(m, "NARS.Derive", &tasks, &beliefs, config[MAX_STEPS].value, NULL);
    derive(&nars, &tasks, &beliefs, 1, config[MAX_STEPS].value, configured);
    assert(atom_is(queues, E(tuple(&tasks), tuple(&beliefs))) && "no task stops the loop");
    mt_list_free(tasks), mt_list_free(beliefs);

    /* Queues bounded at 0 keep nothing: the first selection derives, both
       queues are cut to (), and the empty task queue stops the loop. */
    const bounds nothing = { 0, 0 };
    tasks = QUEUE(mt_keep(premise)), beliefs = QUEUE(mt_keep(belief));
    queues = engine_derive(m, "NARS.Derive", &tasks, &beliefs, config[MAX_STEPS].value, &nothing);
    derive(&nars, &tasks, &beliefs, 1, config[MAX_STEPS].value, nothing);
    assert(atom_is(queues, E(tuple(&tasks), tuple(&beliefs))) && "queues bounded at zero keep nothing");
    mt_list_free(tasks), mt_list_free(beliefs), mt_drop(premise), mt_drop(belief);
    mt_close(m);
    return 0;
}
