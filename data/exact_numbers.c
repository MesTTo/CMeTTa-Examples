/* Purpose: numbers stay exact across the boundary. A uint64_t above INT64_MAX
 *   is a BigInt with every digit, a ratio is stored in lowest terms with the
 *   sign on the numerator, and the engine's arithmetic on ratios is exact.
 * Guarantees: 2^64-1 keeps its digits, 6/-8 is -3/4, and -3/4 + 1/4 is -1/2
 *   [tested: make check; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    mt_atom *wide = mt_unum(UINT64_MAX);
    check("2^64-1 is a BigInt", mt_kind_of(wide) == MT_BIGINT);
    check_text("with every digit", mt_name(wide), "18446744073709551615");

    mt_atom *ratio = mt_rational(6, -8);
    mt_ratio lowest = mt_ratio_of(ratio);
    check("6/-8 is stored as -3/4", lowest.num == -3 && lowest.den == 4);

    mt_atom *sum = mt_one(mt_eval(m, E("+", mt_keep(ratio), mt_rational(1, 4))));
    mt_ratio exact = mt_ratio_of(sum);
    check("-3/4 + 1/4 is exactly -1/2", mt_kind_of(sum) == MT_RATIONAL &&
                                         exact.num == -1 && exact.den == 2);
    check_answers("and 2^64-1 crosses the engine unchanged",
                  mt_eval(m, mt_keep(wide)), mt_unum(UINT64_MAX));
    mt_drop(sum);
    mt_drop(ratio);
    mt_drop(wide);
    return done(m);
}
