/* Purpose: Preserve unsigned integers and canonical rational values.
 * Owns resources: local C handles are released before exit; a failed check
 *   terminates the example process.
 * Guarantees: the assertions below hold [tested: make check; commit=WORKTREE].
 * Open Obligations: rational source round trips are blocked in the shared
 *   reader; this file checks numeric fields directly. See ERRORS.md.
 */
#include "common.h"
int main(void)
{
    metta *m = open_engine();
    mt_atom *big = mt_unum(UINT64_MAX);
    check("unsigned maximum stays exact", mt_kind_of(big) == MT_BIGINT &&
          strcmp(mt_name(big), "18446744073709551615") == 0);
    mt_atom *ratio = mt_rational(6, -8);
    mt_ratio value = mt_ratio_of(ratio);
    check("ratio normalizes sign and factors", value.num == -3 && value.den == 4);
    mt_atom *sum = mt_one(mt_eval(m, mt_expr("+", mt_keep(ratio), mt_rational(1, 4))));
    mt_ratio result = mt_ratio_of(sum);
    check("exact arithmetic", result.num == -1 && result.den == 2);
    mt_drop(sum);
    mt_drop(big); mt_drop(ratio);
    return done(m, "exact_numbers");
}
