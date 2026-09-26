/* Purpose: fib by iteration. fib-step turns the pair (a b) into
 *   (b a+b), and iterate carries the pair n times; C carries the same pair
 *   in two variables. Where int64_t holds the answer the engine must agree
 *   with C; fib 100 does not fit, and arrives as a BIGINT.
 * Guarantees: the original's claim holds, and C agrees at 90
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

static int64_t fib(int64_t n)
{
    int64_t a = 0, b = 1;
    for (int64_t i = 0; i < n; i++) {
        int64_t next = a + b;
        a = b;
        b = next;
    }
    return a;
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_patrick", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_patrick")))));
    require("fib-step", mt_add(m, E("=", E("fib-step", V("i"), E(V("a"), V("b"))), E(V("b"), E("+", V("a"), V("b"))))));
    require("fib", mt_add(m, E("=", E("fib", V("n")), E("first", E("iterate", 0, V("n"), E(0, 1), "fib-step")))));

    assert(answers_are(mt_eval(m, E("fib", 90)), E(fib(90))) && "C and the engine agree where int64_t holds fib");
    assert(answers_are(mt_eval(m, E("fib", 100)), E(mt_bigint("354224848179261915075"))) && "(fib 100)");
    mt_close(m);
    return 0;
}
