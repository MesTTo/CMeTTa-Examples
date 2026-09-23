/* Purpose: an exact rule sharpens uncertain sensing. A C sensor reports a
 *   score for each digit it might be seeing, twice; the rule that the two
 *   digits sum to 3 keeps only consistent pairs, weighted by the product of
 *   their scores, and the posterior for the true reading rises above what
 *   either sensor said alone.
 * Decides: the scores are fixed, so the posterior is reproducible.
 * Guarantees: two hypotheses survive with mass 0.505, and the right one
 *   carries over 90% of it [tested: make check; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"
#include <math.h>

int main(void)
{
    metta *m = open_engine();
    static const double scores[2][3] = { { 0.1, 0.65, 0.25 }, { 0.1, 0.2, 0.7 } };
    static const char *const sensors[2] = { "a", "b" };
    for (size_t s = 0; s < 2; s++)
        for (int64_t digit = 0; digit < 3; digit++)
            require("a sensor reading", mt_add(m, E("sees", sensors[s], digit, scores[s][digit])));
    /* (= (consistent $sum) (match &self (, (sees a $a $wa) (sees b $b $wb))
                              (if (== (+ $a $b) $sum) (Hypothesis $a (* $wa $wb)) Empty))) */
    require("the sum rule", mt_add(m, E("=", E("consistent", V("sum")),
        E("match", "&self", E(",", E("sees", "a", V("a"), V("wa")), E("sees", "b", V("b"), V("wb"))),
          E("if", E("==", E("+", V("a"), V("b")), V("sum")),
            E("Hypothesis", V("a"), E("*", V("wa"), V("wb"))), "Empty")))));

    mt_list hypotheses = mt_all(mt_eval(m, E("consistent", 3)));
    check_int("two readings are consistent with the rule", (int64_t)hypotheses.len, 2);
    double mass = 0.0, right = 0.0;
    for (size_t i = 0; i < hypotheses.len; i++) {
        double weight = mt_float(mt_at(hypotheses.items[i], 2));
        mass += weight;
        if (mt_int(mt_at(hypotheses.items[i], 1)) == 1) right += weight;
    }
    check("their joint mass is 0.505", fabs(mass - 0.505) < 1e-12);
    check("and the right reading carries over 90% of it, more than either sensor",
          right / mass > 0.9 && right / mass > scores[0][1]);
    mt_list_free(hypotheses);
    return done(m);
}
