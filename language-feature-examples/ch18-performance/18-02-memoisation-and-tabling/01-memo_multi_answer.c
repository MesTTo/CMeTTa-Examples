/* Purpose: a memoized function with two answers keeps both. choose's two
 *   equations are lowered from C tokens and memoized once they exist, since
 *   lib_memo instruments a defined function and refuses a name that is not
 *   one yet. The first ask computes x and (Pair x x); the second answers
 *   both from the cache, in the same order, and the library's own counters
 *   say it was a hit.
 * Guarantees: both claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"
#include "tabling.h"

int main(void)
{
    metta *m = open_engine();
    require("import lib_memo", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_memo")))));
    require("(choose $x) is $x", mt_lower(m, (choose $x), $x));
    require("and (Pair $x $x)", mt_lower(m, (choose $x), (Pair $x $x)));
    require("memoize choose", mt_one_truth(mt_eval(m, E("memoize", "choose"))));

    const int64_t x = 7;
    check_answers("the miss computes both answers", mt_eval(m, E("choose", x)), x, E("Pair", x, x));
    check_answers("the hit answers both again", mt_eval(m, E("choose", x)), x, E("Pair", x, x));
    check_answers("one miss, then one hit", mt_eval(m, E("get-memoize-stats")),
                  memo_counters(1, 1));
    return done(m);
}
