/* Purpose: a memoized double. DOUBLE is one body lowering.h's operators
 *   compile to the C function doubled() and lower to the equation, and both
 *   asks, the miss and the hit, answer what C computes.
 * Guarantees: both claims of the original hold, the second ask being a hit
 *   [tested: make twins; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include "lowering.h"
#include "tabling.h"

#define DOUBLE(ADD, x) ADD(x, x)

static int64_t doubled(int64_t x) { return DOUBLE(C_ADD, x); }

int main(void)
{
    metta *m = open_engine();
    require("import lib_memo", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_memo")))));
    require("double", mt_lower(m, (double $x), DOUBLE(M_ADD, $x)));
    require("memoize double", mt_one_truth(mt_eval(m, E("memoize", "double"))));

    check_answers("the miss", mt_eval(m, E("double", 5)), doubled(5));
    check_answers("the hit", mt_eval(m, E("double", 5)), doubled(5));
    check_answers("one of each", mt_eval(m, E("get-memoize-stats")), memo_counters(1, 1));
    return done(m);
}
