/* Purpose: let* binds where an expression stands, which C does with an
 *   assignment, itself an expression: (big = n * 10, big + big) names the
 *   value and reuses it, sequenced by the comma operator, and
 *   (half = n / 2) < 10 tests the value it has just named. The engine's
 *   let* equations must answer what these C functions return.
 * Guarantees: all three claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

static int64_t double_used(int64_t n)
{
    int64_t big;
    return (big = n * 10, big + big);
}

static mt_atom *guarded_half(int64_t n)
{
    int64_t half;
    return (half = n / 2) < 10 ? N(half) : S("nope");     /* n >= 0, so / floors */
}

int main(void)
{
    metta *m = open_engine();
    require("double-used", mt_lower(m, (double-used $n), (let* (($big (* $n 10))) (+ $big $big))));
    require("guarded-half", mt_lower(m, (guarded-half $n),
                                     (let* (($half (floor-div $n 2))) (if (< $half 10) $half nope))));
    check_answers("(double-used 3)", mt_eval(m, E("double-used", 3)), double_used(3));
    check_answers("(guarded-half 8)", mt_eval(m, E("guarded-half", 8)), guarded_half(8));
    check_answers("(guarded-half 40)", mt_eval(m, E("guarded-half", 40)), guarded_half(40));
    return done(m);
}
