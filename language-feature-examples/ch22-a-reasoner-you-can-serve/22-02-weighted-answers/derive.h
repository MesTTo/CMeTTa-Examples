/* Purpose: C's model of the loop NARS and PLN run between inferences, which
 *   lib_nars and lib_pln each write over their own helpers and which is one
 *   algorithm [source 2026-09-24T23:30:03+10:00: lib/lib_nars/lib.metta,
 *   StampDisjoint to NARS.Query; lib/lib_pln/lib.metta, StampDisjoint to
 *   PLN.Query; at superproject 99bd67a73, lib fa808bc]:
 *   - a sentence is (Sentence (term (stv f c)) (id ...)), a term, the truth
 *     held of it and the stamp of evidence behind it, and C keeps it as that
 *     atom, so two sentences are the same exactly when the engine's identity
 *     says so;
 *   - two sentences combine only when their stamps share no ID, and what
 *     they derive carries both stamps merged and sorted;
 *   - a queue hands out its most confident sentence, the first on a tie, and
 *     is bounded by dropping its least confident until it is smaller than
 *     its size or empty, so the empty queue is its own limit at any size;
 *   - the loop runs a step budget down, or stops when no task is left.
 *   The rules are the includer's: what one ordered pair derives, in the
 *   library's rule order, and what one sentence derives alone, each as the
 *   (term (stv f c)) pairs (|- x y) and (|- x) answer.
 *   Beside the model are the builders a twin states its premises with and
 *   the call that asks the engine's loop for its queues.
 * Assumes: the includer includes common.h and nars_truth.h first.
 * Guarantees: derive() leaves the queues the library's loop answers for the
 *   same queues, rules and budgets, and query() the answer the library's
 *   query picks from them [tested 2026-09-24T23:32:18+10:00: make twins].
 * Owns resources: every mt_list here owns its atoms and its array, which
 *   mt_list_free() releases.
 */
#ifndef CH22_DERIVE_H
#define CH22_DERIVE_H

/* The rank PriorityRank and PriorityRankNeg give the empty tuple a fold
   starts from, below any confidence, and the one ConfidenceRank gives it,
   an integer. */
#define SENTINEL (-99999.0)
#define NO_CONFIDENCE 0

typedef struct rules {
    void (*pair)(const mt_atom *x, const mt_atom *y, mt_list *out);
    void (*alone)(const mt_atom *x, mt_list *out);
} rules;

/* The sizes a derivation bounds its task and belief queues to. */
typedef struct bounds { size_t tasks, beliefs; } bounds;

/* Append ATOM, TAKEN. */
static inline void push(mt_list *list, mt_atom *atom)
{
    mt_atom **grown = mt_resize(list->items, (list->len + 1) * sizeof *grown);
    require("room", grown != NULL);
    list->items = grown;
    list->items[list->len++] = atom;
}

static inline bool holds(const mt_list *list, const mt_atom *atom)
{
    for (size_t i = 0; i < list->len; i++)
        if (mt_eq(list->items[i], atom)) return true;
    return false;
}

/* The list as the tuple (item ...), the list kept. */
static inline mt_atom *tuple(const mt_list *list)
{
    mt_atom **items = malloc((list->len ? list->len : 1) * sizeof *items);
    require("room", items != NULL);
    for (size_t i = 0; i < list->len; i++) items[i] = mt_keep(list->items[i]);
    mt_atom *atom = mt_exprv(list->len, items);
    free(items);
    return atom;
}

/* A tuple's children as a list of their own. */
static inline mt_list items_of(const mt_atom *tuple)
{
    mt_list list = { NULL, 0 };
    for (size_t i = 0; i < mt_len(tuple); i++) push(&list, mt_keep(mt_at(tuple, i)));
    return list;
}

/* ---- sentences ---- */

/* (Sentence (TERM (stv f c)) STAMP). TAKES term and stamp. */
static inline mt_atom *sentence(mt_atom *term, truth tv, mt_atom *stamp) { return mt_expr("Sentence", mt_expr(term, stv(tv)), stamp); }

/* A sentence's (term truth) pair, its truth and its stamp. */
static inline const mt_atom *judgement(const mt_atom *s) { return mt_at(s, 1); }
static inline truth truth_of(const mt_atom *judgement) { return stv_truth(mt_at(judgement, 1)); }
static inline const mt_atom *stamp_of(const mt_atom *s) { return mt_at(s, 2); }

/* ---- stamps ---- */

static inline bool stamps_disjoint(const mt_atom *a, const mt_atom *b)
{
    for (size_t i = 0; i < mt_len(a); i++)
        for (size_t j = 0; j < mt_len(b); j++)
            if (mt_eq(mt_at(a, i), mt_at(b, j))) return false;
    return true;
}

/* Both stamps' IDs in the standard order, duplicates kept, as msort sorts. */
static inline mt_atom *stamps_merged(const mt_atom *a, const mt_atom *b)
{
    mt_list ids = items_of(a), more = items_of(b);
    for (size_t i = 0; i < more.len; i++) push(&ids, mt_keep(more.items[i]));
    mt_list_free(more);
    qsort(ids.items, ids.len, sizeof *ids.items, mt_order);
    mt_atom *merged = tuple(&ids);
    mt_list_free(ids);
    return merged;
}

/* StampConcat: an empty addition leaves the stamp as it was, unsorted. */
static inline mt_atom *stamp_concat(const mt_atom *stamp, const mt_atom *addition)
{
    return mt_len(addition) == 0 ? mt_keep(stamp) : stamps_merged(stamp, addition);
}

/* ---- the queue ---- */

/* A queue of the sentences given, TAKEN: QUEUE(s1, s2). */
static inline mt_list queue_of(size_t n, mt_atom **sentences)
{
    mt_list list = { NULL, 0 };
    for (size_t i = 0; i < n; i++) push(&list, sentences[i]);
    return list;
}
#define QUEUE(...) queue_of(MT_NARG(__VA_ARGS__), (mt_atom *[]){ __VA_ARGS__ })

/* PriorityRank and PriorityRankNeg, NULL being the empty tuple. */
static inline double priority(const mt_atom *s) { return s ? truth_of(judgement(s)).c : SENTINEL; }
static inline double priority_neg(const mt_atom *s) { return s ? 0.0 - truth_of(judgement(s)).c : SENTINEL; }

/* BestCandidate: the first item ranked strictly above every one before it,
   starting from the empty tuple; NULL where none is. */
static inline const mt_atom *best_candidate(double (*rank)(const mt_atom *), const mt_list *queue)
{
    const mt_atom *best = NULL;
    for (size_t i = 0; i < queue->len; i++)
        if (rank(queue->items[i]) > rank(best)) best = queue->items[i];
    return best;
}

/* The atom BestCandidate answers: the pick, or the empty tuple it started
   from. */
static inline mt_atom *candidate(const mt_atom *best) { return best ? mt_keep(best) : mt_unit(); }

/* exclude-item: drop every item identical to ITEM. */
static inline void exclude(mt_list *queue, const mt_atom *item)
{
    mt_atom *gone = mt_keep(item);
    size_t kept = 0;
    for (size_t i = 0; i < queue->len; i++)
        if (mt_eq(queue->items[i], gone))
            mt_drop(queue->items[i]);
        else
            queue->items[kept++] = queue->items[i];
    queue->len = kept;
    mt_drop(gone);
}

/* LimitSize: drop the least confident until fewer than SIZE remain or none
   do, so at a size of 0 the queue empties and the empty queue stops it.
   Both libraries test (== $L ()) beside the size since lib 709ab77a7, where
   before it (LimitSize () 0) recursed on its own arguments and never
   answered [source 2026-09-24T23:30:03+10:00: lib/lib_nars/lib.metta:229 and
   lib/lib_pln/lib.metta:429 at superproject 99bd67a73, lib fa808bc]. */
static inline void limit_size(mt_list *queue, size_t size)
{
    while (!(queue->len < size) && queue->len > 0) exclude(queue, best_candidate(priority_neg, queue));
}

/* The tuple LimitSize answers for QUEUE at SIZE, the queue kept. */
static inline mt_atom *limited(const mt_list *queue, size_t size)
{
    mt_list copy = { NULL, 0 };
    for (size_t i = 0; i < queue->len; i++) push(&copy, mt_keep(queue->items[i]));
    limit_size(&copy, size);
    mt_atom *left = tuple(&copy);
    mt_list_free(copy);
    return left;
}

/* list_to_set of two queues appended: first occurrences, in order. */
static inline mt_list set_of(const mt_list *a, const mt_list *b)
{
    mt_list set = { NULL, 0 };
    const mt_list *parts[] = { a, b };
    for (size_t p = 0; p < 2; p++)
        for (size_t i = 0; i < parts[p]->len; i++)
            if (!holds(&set, parts[p]->items[i])) push(&set, mt_keep(parts[p]->items[i]));
    return set;
}

/* ---- the loop ---- */

/* NARS.Derive and PLN.Derive from step STEPS to MAX_STEPS: select the most
   confident task, combine it with every belief whose stamp is disjoint from
   its own, both ways round, add what it derives alone, and bound both
   queues to SIZES. Both queues are updated in place. */
static inline void derive(const rules *r, mt_list *tasks, mt_list *beliefs, int64_t steps, int64_t max_steps, bounds sizes)
{
    for (; !(steps > max_steps) && tasks->len > 0; steps++) {
        const mt_atom *best = best_candidate(priority, tasks);
        require("a task ranks above the empty tuple", best != NULL);
        mt_atom *selected = mt_keep(best);
        const mt_atom *x = judgement(selected), *ev1 = stamp_of(selected);
        mt_list derived = { NULL, 0 };
        for (size_t i = 0; i < beliefs->len; i++) {
            const mt_atom *y = judgement(beliefs->items[i]), *ev2 = stamp_of(beliefs->items[i]);
            if (!stamps_disjoint(ev1, ev2)) continue;
            mt_list found = { NULL, 0 };
            r->pair(x, y, &found);
            r->pair(y, x, &found);
            mt_atom *stamp = stamps_merged(ev1, ev2);
            for (size_t k = 0; k < found.len; k++) push(&derived, mt_expr("Sentence", mt_keep(found.items[k]), mt_keep(stamp)));
            mt_drop(stamp);
            mt_list_free(found);
        }
        mt_list alone = { NULL, 0 };
        if (r->alone) r->alone(x, &alone);
        for (size_t k = 0; k < alone.len; k++) push(&derived, mt_expr("Sentence", mt_keep(alone.items[k]), mt_keep(ev1)));
        mt_list_free(alone);

        mt_list next_tasks = set_of(tasks, &derived), next_beliefs = set_of(beliefs, &derived);
        exclude(&next_tasks, selected);
        limit_size(&next_tasks, sizes.tasks);
        limit_size(&next_beliefs, sizes.beliefs);
        mt_list_free(*tasks), mt_list_free(*beliefs), mt_list_free(derived);
        *tasks = next_tasks, *beliefs = next_beliefs;
        mt_drop(selected);
    }
}

/* ConfidenceRank: a query answer's confidence, the answer being
   ((stv f c) stamp), NULL being the empty tuple. */
static inline double confidence(const mt_atom *answer) { return answer ? stv_truth(mt_at(answer, 0)).c : NO_CONFIDENCE; }

/* NARS.Query and PLN.Query over the beliefs a derivation ended with: every
   belief about TERM as its ((stv f c) stamp), in order, and the one
   BestCandidate picks from them by confidence, or () where none is more
   confident than the empty tuple. */
static inline mt_atom *query(const mt_list *beliefs, const mt_atom *term)
{
    mt_list answers = { NULL, 0 };
    for (size_t i = 0; i < beliefs->len; i++) {
        const mt_atom *s = beliefs->items[i];
        if (mt_eq(mt_at(judgement(s), 0), term)) push(&answers, mt_expr(mt_keep(mt_at(judgement(s), 1)), mt_keep(stamp_of(s))));
    }
    mt_atom *pick = candidate(best_candidate(confidence, &answers));
    mt_list_free(answers);
    return pick;
}

/* (NAME TASKS BELIEFS STEPS) asked of the engine, NAME being NARS.Derive or
   PLN.Derive, with BOUND's two sizes after STEPS, or none, which leaves the
   library's configured ones: the two queues it ends with. */
static inline mt_atom *engine_derive(metta *m, const char *name, const mt_list *tasks, const mt_list *beliefs, int64_t steps,
                                     const bounds *bound)
{
    mt_atom *call = bound ? mt_expr(name, tuple(tasks), tuple(beliefs), steps, (int64_t)bound->tasks, (int64_t)bound->beliefs)
                          : mt_expr(name, tuple(tasks), tuple(beliefs), steps);
    mt_atom *queues = mt_one(mt_eval(m, call));
    require("the loop answers its two queues", queues && mt_len(queues) == 2);
    return queues;
}
#endif
