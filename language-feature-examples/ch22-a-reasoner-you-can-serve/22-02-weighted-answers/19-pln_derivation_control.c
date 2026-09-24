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
 * Guarantees: all twenty-five claims of the original hold [tested: make
 *   twins; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include "nars_truth.h"
#include "pln.h"
#include "derive.h"

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

/* The loop over TASKS and BELIEFS for STEPS steps, run by C, with the
   configured queue sizes. */
static void run_loop(mt_list *tasks, mt_list *beliefs, int64_t steps)
{
    derive(&pln, tasks, beliefs, 1, steps, (size_t)config[TASK_QUEUE_SIZE].value, (size_t)config[BELIEF_QUEUE_SIZE].value);
}

int main(void)
{
    metta *m = open_engine();
    require("lib_pln", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_pln")))));

    for (size_t i = 0; i < sizeof config / sizeof *config; i++) check_answers(config[i].name, mt_eval(m, E(config[i].name)), config[i].value);

    mt_atom *disjoint[][2] = { { E(1, 2), E(3, 4) }, { E(1, 2), E(2, 3) }, { mt_unit(), E(1) } };
    for (size_t i = 0; i < sizeof disjoint / sizeof *disjoint; i++) {
        check_answers("StampDisjoint", mt_eval(m, E("StampDisjoint", mt_keep(disjoint[i][0]), mt_keep(disjoint[i][1]))),
                      B(stamps_disjoint(disjoint[i][0], disjoint[i][1])));
        mt_drop(disjoint[i][0]), mt_drop(disjoint[i][1]);
    }
    mt_atom *concat[][2] = { { E(3, 1), E(2) }, { E(3, 1), mt_unit() } };
    for (size_t i = 0; i < 2; i++) {
        check_answers("StampConcat", mt_eval(m, E("StampConcat", mt_keep(concat[i][0]), mt_keep(concat[i][1]))),
                      stamp_concat(concat[i][0], concat[i][1]));
        mt_drop(concat[i][0]), mt_drop(concat[i][1]);
    }
    check_atom("StampConcat is order-free", mt_one(mt_eval(m, E("StampConcat", E(1), E(2)))),
               mt_one(mt_eval(m, E("StampConcat", E(2), E(1)))));

    mt_atom *quiet = sentence(S("a"), (truth){ 1.0, 0.5 }, E(1)), *loud = sentence(S("b"), (truth){ 1.0, 0.9 }, E(2));
    mt_atom *certain_a = sentence(S("a"), (truth){ 1.0, 0.9 }, E(1));
    check_answers("PriorityRank", mt_eval(m, E("PriorityRank", mt_keep(certain_a))), priority(certain_a));
    check_answers("PriorityRank of ()", mt_eval(m, E("PriorityRank", mt_unit())), priority(NULL));
    check_answers("PriorityRankNeg", mt_eval(m, E("PriorityRankNeg", mt_keep(certain_a))), priority_neg(certain_a));
    check_answers("PriorityRankNeg of ()", mt_eval(m, E("PriorityRankNeg", mt_unit())), priority_neg(NULL));
    mt_atom *answer = E(stv((truth){ 1.0, 0.9 }), E(1));
    check_answers("ConfidenceRank", mt_eval(m, E("ConfidenceRank", mt_keep(answer))), confidence(answer));
    check_answers("ConfidenceRank of ()", mt_eval(m, E("ConfidenceRank", mt_unit())), (int64_t)NO_CONFIDENCE);
    mt_drop(answer), mt_drop(certain_a);

    mt_list pair = QUEUE(mt_keep(quiet), mt_keep(loud)), none = { NULL, 0 };
    check_answers("the most confident", mt_eval(m, E("BestCandidate", "PriorityRank", mt_unit(), tuple(&pair))),
                  candidate(best_candidate(priority, &pair)));
    check_answers("the least confident", mt_eval(m, E("BestCandidate", "PriorityRankNeg", mt_unit(), tuple(&pair))),
                  candidate(best_candidate(priority_neg, &pair)));
    check_answers("nothing to choose from", mt_eval(m, E("BestCandidate", "PriorityRank", mt_unit(), mt_unit())),
                  candidate(best_candidate(priority, &none)));

    mt_list single = QUEUE(mt_keep(quiet));
    check_answers("LimitSize under the bound", mt_eval(m, E("LimitSize", tuple(&single), 5)), limited(&single, 5));
    check_answers("LimitSize at two", mt_eval(m, E("LimitSize", tuple(&pair), 2)), limited(&pair, 2));
    check_answers("LimitSize at one", mt_eval(m, E("LimitSize", tuple(&pair), 1)), limited(&pair, 1));
    mt_list_free(single), mt_list_free(pair), mt_drop(quiet), mt_drop(loud);

    /* An implication and a fact, two steps: modus ponens gives b with both
       IDs, and the loop ends with the queues C's loop ends with. */
    mt_atom *rule = sentence(E("Implication", "a", "b"), (truth){ 1.0, 0.9 }, E(1)), *fact = sentence(S("a"), (truth){ 1.0, 0.9 }, E(2));
    mt_list tasks = QUEUE(mt_keep(rule)), beliefs = QUEUE(mt_keep(fact));
    mt_atom *queues = engine_derive(m, "PLN.Derive", &tasks, &beliefs, 2);
    run_loop(&tasks, &beliefs, 2);
    check_atom("modus ponens joins the beliefs", mt_keep(mt_at(queues, 1)), tuple(&beliefs));
    mt_list_free(tasks), mt_list_free(beliefs), mt_drop(queues);

    tasks = QUEUE(mt_keep(rule)), beliefs = QUEUE(mt_keep(fact));
    queues = engine_derive(m, "PLN.Derive", &tasks, &beliefs, 0);
    run_loop(&tasks, &beliefs, 0);
    check_atom("a budget of zero keeps both queues", queues, E(tuple(&tasks), tuple(&beliefs)));
    mt_list_free(tasks), mt_list_free(beliefs);

    tasks = (mt_list){ NULL, 0 }, beliefs = QUEUE(mt_keep(fact));
    queues = engine_derive(m, "PLN.Derive", &tasks, &beliefs, config[MAX_STEPS].value);
    run_loop(&tasks, &beliefs, config[MAX_STEPS].value);
    check_atom("no task stops the loop", queues, E(tuple(&tasks), tuple(&beliefs)));
    mt_list_free(tasks), mt_list_free(beliefs);

    /* The query: the loop over the knowledge base as both queues, then the
       most confident belief about b. */
    mt_list kb = QUEUE(mt_keep(rule), mt_keep(fact));
    tasks = QUEUE(mt_keep(rule), mt_keep(fact)), beliefs = QUEUE(mt_keep(rule), mt_keep(fact));
    run_loop(&tasks, &beliefs, 2);
    mt_atom *b = S("b");
    check_answers("PLN.Query", mt_eval(m, E("PLN.Query", tuple(&kb), mt_keep(b), 2)), query(&beliefs, b));
    mt_list_free(kb), mt_list_free(tasks), mt_list_free(beliefs), mt_drop(b), mt_drop(rule), mt_drop(fact);
    return done(m);
}
