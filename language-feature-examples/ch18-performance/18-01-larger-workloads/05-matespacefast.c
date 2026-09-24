/* Purpose: the mate-space rewrite as C walks a binary tree. rewriteK stores
 *   (num (M t)), (num (W t)) and (num (C t)) for its term and goes on from
 *   (M t) and (W t) with one step fewer; C walks that tree depth first,
 *   builds each child term once and shares it with its subtree, and stores
 *   all 3 * (2^K - 1) atoms through one mt_add_all batch, where the original
 *   adds three atoms a step. mate-space-demo stays the original's equation,
 *   lowered from C tokens, and the engine counts its answers where they are,
 *   collapsing the million and a half into one list under the seats' 8 GB
 *   stack as the original does; C checks the count against the one the
 *   tree's shape gives. Walking the answers across to C instead converts
 *   every one of them for a count that needs none: 1.64 trillion user
 *   instructions against 1.38 trillion counting in the engine, where the
 *   original runs 1.19 trillion [measured 2026-09-24: perf stat -e
 *   instructions:u, one run of each, the lane report included, the walk
 *   being mt_each over mt_eval of the demo; commit=c2f866159d87e0a713120e3515454f6e44ce2ded].
 * Guarantees: the original's claim holds [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

/* Store the three rewrites of t, then walk on from (M t) and (W t). Depth is
   the step count, so C's own stack holds the walk. TAKES t. */
static void rewrite(mt_atom *t, int64_t steps, mt_list *batch)
{
    if (steps == 0) {
        mt_drop(t);
        return;
    }
    mt_atom *mated = E("M", mt_keep(t)), *walked = E("W", mt_keep(t));
    batch->items[batch->len++] = E("num", mt_keep(mated));
    batch->items[batch->len++] = E("num", mt_keep(walked));
    batch->items[batch->len++] = E("num", E("C", t));
    rewrite(mated, steps - 1, batch);
    rewrite(walked, steps - 1, batch);
}

/* The atoms rewriteK stores over `steps` steps: three per rewritten term, and
   2^steps - 1 terms. */
static size_t rewritten(int64_t steps) { return 3 * (((size_t)1 << steps) - 1); }

/* What (rewriteK t n) answers: done at 0, else the pair of its two walks'
   answers, one shared subtree standing for both halves. */
static mt_atom *walks(int64_t steps)
{
    if (steps == 0) return S("done");
    mt_atom *half = walks(steps - 1);
    return E(mt_keep(half), half);
}

/* The original counts down to 0, which a negative count never reaches, so
   that is refused by name, and so is a count whose atoms no array this
   process can allocate would hold. */
static mt_status rewrite_k(mt_call *call, void *user)
{
    (void)user;
    if (mt_kind_of(mt_arg(call, 1)) != MT_INT) return MT_FAIL;
    const int64_t steps = mt_int(mt_arg(call, 1));
    if (steps < 0) return mt_fail(call, "rewriteK counts down to 0, which a negative count never reaches");
    if (steps >= (int64_t)(8 * sizeof(size_t) - 2) || rewritten(steps) > SIZE_MAX / sizeof(mt_atom *))
        return mt_error_set(MT_NOMEM, "rewriteK's 3 * (2^n - 1) atoms are more than an array can hold");
    mt_list batch = { mt_alloc(rewritten(steps) * sizeof *batch.items), 0 };
    if (rewritten(steps) > 0 && !batch.items) return mt_error_set(MT_NOMEM, "rewriteK has no room for its batch");
    rewrite(mt_keep(mt_arg(call, 0)), steps, &batch);
    return mt_add_all(mt_of(call), batch) ? mt_answer(call, walks(steps)) : mt_error();
}

int main(void)
{
    metta *m = open_engine();
    require("rewriteK", mt_def(m, (mt_op){ .name = "rewriteK", .arity = 2, .effect = MT_EFFECT_CLASS_WRITES_STATE,
                                           .fn = rewrite_k }));
    require("mate-space-demo", mt_lower(m, (mate-space-demo $K),
                                        (let* (($s (add-atom &self (num Z))) ($g (rewriteK Z $K)))
                                              (match &self (num $1) (num $1)))));
    const int64_t steps = 19;
    check_answers("every (num $1) the demo stored answers once",
                  mt_eval(m, E("length", E("collapse", E("mate-space-demo", steps)))), (int64_t)rewritten(steps) + 1);
    return done(m);
}
