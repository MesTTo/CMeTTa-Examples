/* Purpose: what the cache counted. SQUARE is one body the C_ and T_ operators
 *   compile to the C function square() and build as sq's equation; sq is
 *   memoized and asked three times on one key, each answer what C computes,
 *   and the library's counters say one miss and two hits, over one stored
 *   entry holding one answer.
 * Guarantees: all three claims of the original hold, and the counters agree
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
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
#define C_MUL(a, b) ((a) * (b))
#define T_MUL(a, b) mt_expr("*", a, b)

#define SQUARE(MUL, x) MUL(x, x)

static int64_t square(int64_t x) { return SQUARE(C_MUL, x); }

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_memo", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_memo")))));
    require("sq", mt_add(m, E("=", E("sq", V("x")), SQUARE(T_MUL, V("x")))));
    require("memoize sq", mt_one_truth(mt_eval(m, E("memoize", "sq"))));

    for (int i = 0; i < 3; i++) assert(answers_are(mt_eval(m, E("sq", 9)), E(square(9))) && "(sq 9)");
    assert(answers_are(mt_eval(m, E("get-memoize-stats")), E(memo_counters(2, 1)))
           && "one miss, then two hits");
    assert(answers_are(mt_eval(m, E("get-memoize-stats", "sq")), E(memo_store(1, 1)))
           && "over one entry with one answer");
    mt_close(m);
    return 0;
}
