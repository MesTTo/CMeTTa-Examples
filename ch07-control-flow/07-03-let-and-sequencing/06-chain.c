/* Purpose: chain names its result. It is C's assignment in sequence: scaled
 *   and summed name each intermediate value once, and the engine's chains
 *   over the same arithmetic must answer what they return.
 * Guarantees: both claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>

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

static int64_t scaled(void)
{
    const int64_t n = 2 + 4;
    return 3 * n;
}

static int64_t summed(void)
{
    const int64_t n = 1 + 3;
    const int64_t doubled = 2 * n;
    return n + doubled;
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    assert(answers_are(mt_eval(m, E("chain", E("+", 2, 4), V("n"), E("*", 3, V("n")))), E(scaled())) && "one name");
    assert(answers_are(mt_eval(m, E("chain", E("+", 1, 3), V("n"),
                                    E("chain", E("*", 2, V("n")), V("m"), E("+", V("n"), V("m"))))), E(summed()))
           && "two names");
    mt_close(m);
    return 0;
}
