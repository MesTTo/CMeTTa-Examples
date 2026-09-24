/* Purpose: a triangular walk under iterate. quad-step walks (t i sum) over
 *   the lower triangle of 1000 rows, adding t*i at each cell; C walks the
 *   same triangle with two nested loops, and the engine's sum must be C's.
 * Guarantees: the original's claim holds [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

static int64_t quad_sum(int64_t n)
{
    int64_t sum = 0;
    for (int64_t t = 1; t <= n; t++)
        for (int64_t i = 1; i <= t; i++) sum += t * i;
    return sum;
}

int main(void)
{
    metta *m = open_engine();
    require("import lib_patrick", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_patrick")))));
    require("quad-step", mt_lower(m, (quad-step $dummy ($t $i $sum)),
                                  (if (== $i $t) ((+ $t 1) 1 (+ $sum (* $t $i))) ($t (+ $i 1) (+ $sum (* $t $i))))));
    require("quad-sum", mt_lower(m, (quad-sum $n), (last (iterate 0 (/ (* $n (+ $n 1)) 2) (1 1 0) quad-step))));
    check_answers("(quad-sum 1000)", mt_eval(m, E("quad-sum", 1000)), quad_sum(1000));
    return done(m);
}
