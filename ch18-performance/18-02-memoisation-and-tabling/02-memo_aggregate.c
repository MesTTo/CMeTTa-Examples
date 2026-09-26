/* Purpose: an aggregating cache folds a function's answers into one value.
 *   choices answers x, x + 1 and x + 2, each an equation built from a body
 *   the C_ and T_ operators also compile, and with lib_memo told to aggregate
 *   by sum a ground call answers the sum, which C folds from the same three
 *   bodies. The aggregate is named from vocabularies.h, the engine's own
 *   MemoAggregate words, and set back to none afterwards, as the original
 *   restores it. The Python seat declines this claim, because its compiled
 *   function answers apart from the cache's fold; the C function never
 *   stands in for the equations, so the fold is the engine's.
 * Guarantees: the original's claim holds [tested 2026-09-27T00:35:58+10:00:
 *   make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>

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
#define C_ADD(a, b) ((a) + (b))
#define T_ADD(a, b) mt_expr("+", a, b)

#define CHOICE(ADD, x, k) ADD(x, k)

static mt_atom *aggregate(enum mt_memo_aggregate how)
{
    return E("aggregate", S(mt_memo_aggregate_names[how]));
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_memo", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_memo")))));
    require("choices is $x", mt_add(m, E("=", E("choices", V("x")), V("x"))));
    require("and x + 1", mt_add(m, E("=", E("choices", V("x")), CHOICE(T_ADD, V("x"), 1))));
    require("and x + 2", mt_add(m, E("=", E("choices", V("x")), CHOICE(T_ADD, V("x"), 2))));
    require("aggregate by sum", mt_one_truth(mt_eval(m, E("config-memoize", aggregate(MT_MEMO_AGGREGATE_SUM)))));
    require("memoize choices", mt_one_truth(mt_eval(m, E("memoize", "choices"))));

    const int64_t x = 5;
    assert(answers_are(mt_eval(m, E("choices", x)), E(x + CHOICE(C_ADD, x, 1) + CHOICE(C_ADD, x, 2)))
           && "a ground call answers the sum of its answers");
    require("aggregate by none again", mt_one_truth(mt_eval(m, E("config-memoize", aggregate(MT_MEMO_AGGREGATE_NONE)))));
    mt_close(m);
    return 0;
}
