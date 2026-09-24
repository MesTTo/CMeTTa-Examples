/* Purpose: an aggregating cache folds a function's answers into one value.
 *   choices answers x, x + 1 and x + 2, each an equation lowered from a body
 *   lowering.h's operators also compile, and with lib_memo told to aggregate
 *   by sum a ground call answers the sum, which C folds from the same three
 *   bodies. The aggregate is named from vocabularies.h, the engine's own
 *   MemoAggregate words, and set back to none afterwards, as the original
 *   restores it. The Python seat declines this claim, because its compiled
 *   function answers apart from the cache's fold; the C function never
 *   stands in for the equations, so the fold is the engine's.
 * Guarantees: the original's claim holds [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include "lowering.h"

#define CHOICE(ADD, x, k) ADD(x, k)

static mt_atom *aggregate(enum mt_memo_aggregate how)
{
    return E("aggregate", S(mt_memo_aggregate_names[how]));
}

int main(void)
{
    metta *m = open_engine();
    require("import lib_memo", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_memo")))));
    require("choices is $x", mt_lower(m, (choices $x), $x));
    require("and x + 1", mt_lower(m, (choices $x), CHOICE(M_ADD, $x, 1)));
    require("and x + 2", mt_lower(m, (choices $x), CHOICE(M_ADD, $x, 2)));
    require("aggregate by sum", mt_one_truth(mt_eval(m, E("config-memoize", aggregate(MT_MEMO_AGGREGATE_SUM)))));
    require("memoize choices", mt_one_truth(mt_eval(m, E("memoize", "choices"))));

    const int64_t x = 5;
    check_answers("a ground call answers the sum of its answers", mt_eval(m, E("choices", x)),
                  x + CHOICE(C_ADD, x, 1) + CHOICE(C_ADD, x, 2));
    require("aggregate by none again", mt_one_truth(mt_eval(m, E("config-memoize", aggregate(MT_MEMO_AGGREGATE_NONE)))));
    return done(m);
}
