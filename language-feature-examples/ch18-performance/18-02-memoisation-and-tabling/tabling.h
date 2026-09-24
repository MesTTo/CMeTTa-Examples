/* Purpose: the reports lib_memo and lib_tabling answer, built in C from the
 *   numbers a twin expects, so each twin states a count once and the shape
 *   of the report lives here: the memo counters, one memoized function's
 *   store, and one tabled call's statistics with the policy its standing
 *   declaration carries, spelled from vocabularies.h's cache-policy words.
 * Assumes: the includer defines MT_SHORTHAND and includes common.h first.
 * Guarantees: each builder answers the atom the engine answers for the same
 *   numbers [tested: make twins; commit=WORKTREE].
 */
#ifndef CH18_TABLING_H
#define CH18_TABLING_H

/* (get-memoize-stats): the hit and miss counters. A counter appears once it
   has counted, so a zero is absent rather than 0, and after
   clear-memoize-stats the report is (). */
static inline mt_atom *memo_counters(int64_t hits, int64_t misses)
{
    mt_atom *items[2];
    size_t n = 0;
    if (hits) items[n++] = E("cache_hit", hits);
    if (misses) items[n++] = E("cache_miss", misses);
    return mt_exprv(n, items);
}

/* (get-memoize-stats f): what one memoized function has stored. */
static inline mt_atom *memo_store(int64_t entries, int64_t answers)
{
    return E(E("entries", entries), E("answers", answers));
}

/* (table-stats call): SWI's counters for the call's tables, and the policy
   pair while a declaration stands, which untabled takes away with it. */
typedef struct table_stats {
    int64_t tables, answers, complete_call, invalidated, reevaluated;
    bool declared;
} table_stats;

static inline mt_atom *table_stats_atom(table_stats s)
{
    mt_atom *items[6] = {
        E("tables", s.tables), E("answers", s.answers), E("complete-call", s.complete_call),
        E("invalidated", s.invalidated), E("reevaluated", s.reevaluated),
    };
    size_t n = 5;
    if (s.declared)
        items[n++] = E("policy", E(S(mt_cache_policy_names[MT_CACHE_POLICY_INCREMENTAL]),
                                   S(mt_cache_policy_names[MT_CACHE_POLICY_SHARED])));
    return mt_exprv(n, items);
}
#endif
