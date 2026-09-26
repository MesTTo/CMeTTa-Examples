/* Purpose: the accumulator fib, written once in fibsmart.h and run in both
 *   languages. Where int64_t holds the answer the engine must agree with the
 *   C function; fib 100 does not fit, so the engine's unbounded integer
 *   arrives as a BIGINT, which mt_bigint spells by its digits.
 * Guarantees: the original's claim holds, and the C function agrees at 90
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include "_fixtures/fibsmart.h"

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

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("install the accumulator fib", install_fibsmart(m));
    assert(answers_are(mt_eval(m, E("fib", 90)), E(fib(90))) && "C and the engine agree where int64_t holds fib");
    assert(answers_are(mt_eval(m, E("fib", 100)), E(mt_bigint("354224848179261915075"))) && "(fib 100) outgrows int64_t");
    mt_close(m);
    return 0;
}
