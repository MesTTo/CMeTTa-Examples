/* Purpose: memoization is per arity. add has a two-place and a three-place
 *   equation, each a body the C_ and T_ operators compile to a C function
 *   and build as the equation, and only the two-place one is memoized. Every
 *   call must answer what C computes, the cached arity on its miss and its
 *   hit alike, and the other arity untouched by the cache.
 * Guarantees: all five claims of the original hold, and only the two-place
 *   calls reach the cache [tested 2026-09-27T00:35:58+10:00:
 *   make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
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
#define C_ADD(a, b) ((a) + (b))
#define T_ADD(a, b) mt_expr("+", a, b)

#define ADD2(ADD, x, y) ADD(x, y)
#define ADD3(ADD, x, y, z) ADD(ADD(x, y), z)

static int64_t add2(int64_t x, int64_t y) { return ADD2(C_ADD, x, y); }
static int64_t add3(int64_t x, int64_t y, int64_t z) { return ADD3(C_ADD, x, y, z); }

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_memo", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_memo")))));
    require("add/2", mt_add(m, E("=", E("add", V("x"), V("y")), ADD2(T_ADD, V("x"), V("y")))));
    require("add/3", mt_add(m, E("=", E("add", V("x"), V("y"), V("z")), ADD3(T_ADD, V("x"), V("y"), V("z")))));
    require("memoize add/2", mt_one_truth(mt_eval(m, E("memoize", "add", 2))));

    assert(answers_are(mt_eval(m, E("add", 3, 4)), E(add2(3, 4))) && "(add 3 4) misses");
    assert(answers_are(mt_eval(m, E("add", 3, 4)), E(add2(3, 4))) && "(add 3 4) hits");
    assert(answers_are(mt_eval(m, E("add", 1, 2, 3)), E(add3(1, 2, 3))) && "(add 1 2 3) is not cached");
    assert(answers_are(mt_eval(m, E("add", 5, 6)), E(add2(5, 6))) && "(add 5 6) misses");
    assert(answers_are(mt_eval(m, E("add", 5, 6)), E(add2(5, 6))) && "(add 5 6) hits");
    assert(answers_are(mt_eval(m, E("get-memoize-stats")), E(memo_counters(2, 2)))
           && "two misses and two hits, all two-place");
    mt_close(m);
    return 0;
}
