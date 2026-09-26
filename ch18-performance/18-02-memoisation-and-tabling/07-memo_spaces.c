/* Purpose: memoization belongs to a space. &self and &metric each define
 *   shipping-cost at their own rate, 2 and 9, one body COST expanded two
 *   ways through its operators: with C's operators it is what C expects, and
 *   with the atom builders the equation's atom, which mt_add installs in each
 *   space and mt_del removes from &self when its rate changes to 3. Memoizing in one space leaves the other's function
 *   alone until it is memoized too; the change invalidates only &self's cache.
 *   evalc is mt_eval with the space's handle as its target.
 * Guarantees: all sixteen claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
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
#define C_MUL(a, b) ((a) * (b))
#define T_MUL(a, b) mt_expr("*", a, b)

#define COST(MUL, w, rate) MUL(w, rate)

static int64_t cost(int64_t w, int64_t rate) { return COST(C_MUL, w, rate); }

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_memo", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_memo")))));
    mt_space *metric = mt_space_open(m, "&metric");
    require("open &metric", metric != NULL);
    require("&metric's rate", mt_add(metric, E("=", E("shipping-cost", V("w")), COST(T_MUL, V("w"), 9))));
    require("&self's rate", mt_add(m, E("=", E("shipping-cost", V("w")), COST(T_MUL, V("w"), 2))));

    const int64_t w = 3;
    assert(answers_are(mt_eval(m, E("shipping-cost", w)), E(cost(w, 2))) && "&self's function");
    assert(answers_are(mt_eval(metric, E("shipping-cost", w)), E(cost(w, 9))) && "&metric's");
    assert(answers_are(mt_eval(m, E("is-memoized", "shipping-cost")), E(B(false))) && "neither memoized here");
    assert(answers_are(mt_eval(metric, E("is-memoized", "shipping-cost")), E(B(false))) && "nor there");

    require("memoize &self's", mt_one_truth(mt_eval(m, E("memoize", "shipping-cost"))));
    assert(answers_are(mt_eval(m, E("is-memoized", "shipping-cost")), E(B(true))) && "&self's is memoized");
    assert(answers_are(mt_eval(metric, E("is-memoized", "shipping-cost")), E(B(false))) && "&metric's is not");
    for (int i = 0; i < 2; i++) assert(answers_are(mt_eval(m, E("shipping-cost", w)), E(cost(w, 2))) && "&self's, missed then hit");
    for (int i = 0; i < 2; i++) assert(answers_are(mt_eval(metric, E("shipping-cost", w)), E(cost(w, 9))) && "&metric's, uncached");

    require("memoize &metric's", mt_one_truth(mt_eval(metric, E("memoize", "shipping-cost"))));
    assert(answers_are(mt_eval(metric, E("is-memoized", "shipping-cost")), E(B(true))) && "a second cache");
    for (int i = 0; i < 2; i++) assert(answers_are(mt_eval(metric, E("shipping-cost", w)), E(cost(w, 9))) && "&metric's, missed then hit");
    assert(answers_are(mt_eval(m, E("shipping-cost", w)), E(cost(w, 2))) && "&self's cache still its own");

    require("remove &self's rate", mt_del(m, E("=", E("shipping-cost", V("w")), COST(T_MUL, V("w"), 2))));
    require("its new rate", mt_add(m, E("=", E("shipping-cost", V("w")), COST(T_MUL, V("w"), 3))));
    assert(answers_are(mt_eval(m, E("shipping-cost", w)), E(cost(w, 3))) && "the change invalidates &self's cache");
    assert(answers_are(mt_eval(metric, E("shipping-cost", w)), E(cost(w, 9))) && "and leaves &metric's standing");
    mt_space_close(metric);
    mt_close(m);
    return 0;
}
