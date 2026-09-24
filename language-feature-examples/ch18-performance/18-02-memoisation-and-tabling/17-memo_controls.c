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
 *   compared whole [tested: make twins; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"
#include "lowering.h"
#include "tabling.h"

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
    check_answers("sq answers what C computes", mt_run_goal(engine, E("sq", x)), square(x));
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
    check_answers(claim, mt_run_goal(engine, E("get-memoize-stats")), memo_counters(model->hits, model->misses));
}

static void stores(const char *claim, const memo_model *model)
{
    check_answers(claim, mt_run_goal(engine, E("get-memoize-stats", "sq")),
                  memo_store((int64_t)model->stored, (int64_t)model->stored));
}

static void still_memoized(void)
{
    check_answers("the decision to cache stands", mt_run_goal(engine, E("is-memoized", "sq")), B(true));
}

static void control(const char *name) { require(name, mt_one_truth(mt_run_goal(engine, E(name)))); }

int main(void)
{
    metta *m = engine = open_engine();
    require("import lib_memo", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_memo")))));
    require("(: sq (-> Number Number))", mt_add(m, E(":", "sq", E("->", "Number", "Number"))));
    require("sq", mt_lower(m, (sq $x), SQUARE(M_MUL, $x)));
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
    check("the configuration holds its six settings in order", keyed);
    enum mt_memo_strategy strategy;
    check("the strategy is one of the engine's words",
          keyed && mt_memo_strategy_of(mt_name(mt_at(mt_at(config, 0), 1)), &strategy));
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
    return done(m);
}
