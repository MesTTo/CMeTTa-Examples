/* Purpose: sharpen uncertain C sensor scores with an exact symbolic sum rule.
 * Owns resources: atom constructors transfer scores to the engine.
 * Decides: fixed sensor observations make the posterior check reproducible.
 * Guarantees: the sum constraint raises the correct digit's probability
 *   [tested: make check; commit=WORKTREE].
 * Open Obligations: None.
 */
#include "common.h"
#include <math.h>
int main(void)
{
    metta *m = open_engine();
    const double scores[][3] = {{0.1,0.65,0.25},{0.1,0.2,0.7}};
    for (size_t observation = 0; observation < 2; ++observation)
        for (size_t digit = 0; digit < 3; ++digit)
            check("sensor hypothesis", mt_add(m, mt_expr("sees", observation == 0 ? "a" : "b", (int64_t)digit, scores[observation][digit])));
    check("weighted join rule", mt_do(m,
        "(= (consistent $sum) (match &self (, (sees a $a $wa) (sees b $b $wb)) "
        "(if (== (+ $a $b) $sum) (Hypothesis $a (* $wa $wb)) Empty)))"));
    mt_list rows = mt_all(mt_run(m, "!(consistent 3)"));
    check("constraint keeps two hypotheses", mt_ok() && rows.len == 2);
    double mass = 0.0, correct = 0.0;
    for (size_t i = 0; i < rows.len; ++i) {
        double weight = mt_float(mt_at(rows.items[i], 2)); mass += weight;
        if (mt_int(mt_at(rows.items[i], 1)) == 1) correct += weight;
    }
    check("posterior mass", fabs(mass - 0.505) < 1e-12);
    check("exact constraint sharpens perception", correct / mass > 0.9 && correct / mass > scores[0][1]);
    mt_list_free(rows); return done(m, "uncertain_perception");
}
