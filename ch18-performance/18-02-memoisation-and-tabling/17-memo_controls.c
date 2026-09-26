/* Purpose: lib_memo's controls, held against C's model of what they do. The
 *   model is sq's cache as C keeps it: the keys stored, one answer each, and
 *   the global hit and miss counters. A call on a stored key is a hit, any
 *   other a miss storing its key; clear-memoize-stats deletes the counters,
 *   so the report is empty until something counts again; invalidate-memoize
 *   and clear-memoize drop the stored keys and leave the decision to cache
 *   standing. After each step the engine's reports must be the model's,
 *   built by tabling.h. Every ask runs through mt_run_goal, in the runtime's
 *   own engine: memoize-exact keeps each answer bag in an SWI table private
 *   to the engine that computed it, so through mt_eval's cursors, each in an
 *   engine of its own, every call would miss and the store would read empty.
 *   The configuration is read as C reads a record: its keys in the order the
 *   library reports them, and a strategy that is one of the engine's
 *   MemoStrategy words.
 * Guarantees: all twenty-one claims of the original hold, with each report
 *   compared whole [tested 2026-09-27T00:35:58+10:00:
 *   make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_fixtures/tabling.h"

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

/* Each MeTTa operator twice, for a body written once over its operators:
   C_X computes in C, and T_X builds the atom (X a b). */
#define C_MUL(a, b) ((a) * (b))
#define T_MUL(a, b) mt_expr("*", a, b)

#define SQUARE(MUL, x) MUL(x, x)

static int64_t square(int64_t x) { return SQUARE(C_MUL, x); }

typedef struct memo_model {
    int64_t hits, misses;
    int64_t keys[8];
    size_t stored;
} memo_model;

static metta *engine;

/* (sq x) through the cache: its answer must be C's, and the model counts it. */
static void ask(memo_model *model, int64_t x)
{
    assert(answers_are(mt_run_goal(engine, E("sq", x)), E(square(x))) && "sq answers what C computes");
    for (size_t i = 0; i < model->stored; i++)
        if (model->keys[i] == x) {
            model->hits++;
            return;
        }
    model->misses++;
    model->keys[model->stored++] = x;
}

static void reports(const char *claim, const memo_model *model)
{
    assert(answers_are(mt_run_goal(engine, E("get-memoize-stats")), E(memo_counters(model->hits, model->misses))) && claim);
}

static void stores(const char *claim, const memo_model *model)
{
    assert(answers_are(mt_run_goal(engine, E("get-memoize-stats", "sq")), E(memo_store((int64_t)model->stored, (int64_t)model->stored)))
           && claim);
}

static void still_memoized(void)
{
    assert(answers_are(mt_run_goal(engine, E("is-memoized", "sq")), E(B(true))) && "the decision to cache stands");
}

static void control(const char *name) { require(name, mt_one_truth(mt_run_goal(engine, E(name)))); }

int main(void)
{
    metta *m = engine = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_memo", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_memo")))));
    require("(: sq (-> Number Number))", mt_add(m, E(":", "sq", E("->", "Number", "Number"))));
    require("sq", mt_add(m, E("=", E("sq", V("x")), SQUARE(T_MUL, V("x")))));
    require("memoize-exact sq", mt_one_truth(mt_run_goal(m, E("memoize-exact", "sq"))));
    still_memoized();

    memo_model model = {0};
    ask(&model, 9);
    ask(&model, 9);

    static const char *const settings[] = { "strategy", "unique-limit", "size-limit", "float", "answer-limit", "aggregate" };
    mt_atom *config = mt_first(mt_run_goal(m, E("get-memoize-config")));
    require("the configuration", config != NULL && mt_kind_of(config) == MT_EXPR);
    bool keyed = mt_len(config) == sizeof settings / sizeof *settings;
    for (size_t i = 0; keyed && i < mt_len(config); i++)
        keyed = mt_len(mt_at(config, i)) == 2 && strcmp(mt_name(mt_at(mt_at(config, i), 0)), settings[i]) == 0;
    assert(keyed && "the configuration holds its six settings in order");
    enum mt_memo_strategy strategy;
    assert(keyed && mt_memo_strategy_of(mt_name(mt_at(mt_at(config, 0), 1)), &strategy)
           && "the strategy is one of the engine's words");
    mt_drop(config);

    reports("the counters so far", &model);
    stores("one stored key", &model);
    ask(&model, 4);
    stores("a second key adds an entry", &model);

    control("clear-memoize-stats");
    model.hits = model.misses = 0;
    reports("the counters are deleted, not zeroed", &model);
    ask(&model, 9);
    reports("and count again from the next call", &model);
    stores("the answers survive clearing the counters", &model);

    require("invalidate sq", mt_one_truth(mt_run_goal(m, E("invalidate-memoize", "sq"))));
    model.stored = 0;
    stores("invalidation drops sq's answers", &model);
    still_memoized();
    ask(&model, 9);
    stores("and the next call caches again", &model);

    control("clear-memoize");
    model.stored = 0;
    stores("clear-memoize drops every space's answers", &model);
    still_memoized();
    ask(&model, 9);
    stores("and caching resumes", &model);
    mt_close(m);
    return 0;
}
