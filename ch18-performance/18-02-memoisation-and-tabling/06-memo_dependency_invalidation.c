/* Purpose: a memoized double. DOUBLE is one body the C_ and T_ operators
 *   compile to the C function doubled() and build as the equation, and both
 *   asks, the miss and the hit, answer what C computes.
 * Guarantees: both claims of the original hold, the second ask being a hit
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
#define C_ADD(a, b) ((a) + (b))
#define T_ADD(a, b) mt_expr("+", a, b)

#define DOUBLE(ADD, x) ADD(x, x)

static int64_t doubled(int64_t x) { return DOUBLE(C_ADD, x); }

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_memo", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_memo")))));
    require("double", mt_add(m, E("=", E("double", V("x")), DOUBLE(T_ADD, V("x")))));
    require("memoize double", mt_one_truth(mt_eval(m, E("memoize", "double"))));

    assert(answers_are(mt_eval(m, E("double", 5)), E(doubled(5))) && "the miss");
    assert(answers_are(mt_eval(m, E("double", 5)), E(doubled(5))) && "the hit");
    assert(answers_are(mt_eval(m, E("get-memoize-stats")), E(memo_counters(1, 1))) && "one of each");
    mt_close(m);
    return 0;
}
