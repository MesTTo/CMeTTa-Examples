/* Purpose: a cache keyed by a call's shape. shape-kind answers pair for any
 *   (Pair x y), and memoized it is asked twice with a variable where x goes,
 *   under two different names; the two calls are variants of each other, so
 *   the second is the first's key, and both answer pair.
 * Guarantees: both claims of the original hold, the second ask being a hit
 *   [tested: make twins; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include "tabling.h"

int main(void)
{
    metta *m = open_engine();
    require("import lib_memo", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_memo")))));
    require("shape-kind", mt_add(m, E("=", E("shape-kind", E("Pair", V("x"), V("y"))), S("pair"))));
    require("memoize shape-kind", mt_one_truth(mt_eval(m, E("memoize", "shape-kind"))));

    check_answers("(Pair $a 2) is a pair", mt_eval(m, E("shape-kind", E("Pair", V("a"), 2))), S("pair"));
    check_answers("and (Pair $b 2) the same call", mt_eval(m, E("shape-kind", E("Pair", V("b"), 2))), S("pair"));
    check_answers("so the second was a hit", mt_eval(m, E("get-memoize-stats")),
                  memo_counters(1, 1));
    return done(m);
}
