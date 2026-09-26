/* Purpose: one name at two arities is two functions to lib_memo. mix/1 and
 *   mix/2 are bodies the C_ and T_ operators compile to C functions and build
 *   as the equations; memoizing mix/1 leaves mix/2 unmemoized until it is
 *   memoized too, and is-memoized answers each arity with a boolean. Every
 *   call answers what C computes.
 * Guarantees: all nine claims of the original hold
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
#define C_ADD(a, b) ((a) + (b))
#define T_ADD(a, b) mt_expr("+", a, b)

#define MIX1(ADD, x) ADD(x, 1)
#define MIX2(ADD, x, y) ADD(x, y)

static int64_t mix1(int64_t x) { return MIX1(C_ADD, x); }
static int64_t mix2(int64_t x, int64_t y) { return MIX2(C_ADD, x, y); }

static void memoized(metta *m, int64_t arity, bool is)
{
    assert(answers_are(mt_eval(m, E("is-memoized", "mix", arity)), E(B(is)))
           && (is ? "that arity is memoized" : "that arity is not"));
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_memo", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_memo")))));
    require("mix/1", mt_add(m, E("=", E("mix", V("x")), MIX1(T_ADD, V("x")))));
    require("mix/2", mt_add(m, E("=", E("mix", V("x"), V("y")), MIX2(T_ADD, V("x"), V("y")))));
    require("memoize mix/1", mt_one_truth(mt_eval(m, E("memoize", "mix", 1))));
    memoized(m, 1, true);
    memoized(m, 2, false);

    for (int i = 0; i < 2; i++) assert(answers_are(mt_eval(m, E("mix", 5)), E(mix1(5))) && "(mix 5)");
    for (int i = 0; i < 2; i++) assert(answers_are(mt_eval(m, E("mix", 3, 4)), E(mix2(3, 4))) && "(mix 3 4)");

    require("memoize mix/2", mt_one_truth(mt_eval(m, E("memoize", "mix", 2))));
    memoized(m, 2, true);
    for (int i = 0; i < 2; i++) assert(answers_are(mt_eval(m, E("mix", 8, 9)), E(mix2(8, 9))) && "(mix 8 9)");
    mt_close(m);
    return 0;
}
