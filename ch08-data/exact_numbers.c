/* Purpose: numbers stay exact across the boundary. A uint64_t above INT64_MAX
 *   is a BigInt with every digit, a ratio is stored in lowest terms with the
 *   sign on the numerator, and the engine's arithmetic on ratios is exact.
 * Guarantees: 2^64-1 keeps its digits, 6/-8 is -3/4, and -3/4 + 1/4 is -1/2
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <string.h>

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
    mt_atom *wide = mt_unum(UINT64_MAX);
    assert(mt_kind_of(wide) == MT_BIGINT && "2^64-1 is a BigInt");
    assert(strcmp(mt_name(wide), "18446744073709551615") == 0 && "with every digit");

    mt_atom *ratio = mt_rational(6, -8);
    mt_ratio lowest = mt_ratio_of(ratio);
    assert(lowest.num == -3 && lowest.den == 4 && "6/-8 is stored as -3/4");

    mt_atom *sum = mt_one(mt_eval(m, E("+", mt_keep(ratio), mt_rational(1, 4))));
    mt_ratio exact = mt_ratio_of(sum);
    assert(mt_kind_of(sum) == MT_RATIONAL &&
            exact.num == -1 && exact.den == 2
           && "-3/4 + 1/4 is exactly -1/2");
    assert(answers_are(mt_eval(m, mt_keep(wide)), E(mt_unum(UINT64_MAX)))
           && "and 2^64-1 crosses the engine unchanged");
    mt_drop(sum);
    mt_drop(ratio);
    mt_drop(wide);
    mt_close(m);
    return 0;
}
