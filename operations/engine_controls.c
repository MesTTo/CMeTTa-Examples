/* Purpose: bound and measure. An endless generator is stopped by an inference
 *   budget, which a cursor reports as MT_LIMIT rather than as exhaustion or a
 *   fault, and two samples of the engine's counters price a finite question.
 * Guarantees: the bound stops the stream with MT_LIMIT, and a measured
 *   evaluation spends inferences [tested: make check; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    /* (= (from $n) (superpose ($n (from (+ $n 1))))) */
    require("define an endless generator", mt_add(m, E("=", E("from", V("n")),
        E("superpose", E(V("n"), E("from", E("+", V("n"), 1)))))));
    require("bound every cursor", mt_limit(m, (mt_limits){ .inferences = 5000 }));
    mt_answers *answers = mt_eval(m, E("from", 0));
    require("open the cursor", answers != NULL);
    const mt_atom *answer;
    mt_status status;
    size_t taken = 0;
    while ((status = mt_step(answers, &answer)) == MT_ROW) taken++;
    check("the bound stops it, and says so", status == MT_LIMIT && mt_error() == MT_LIMIT);
    check("after some answers", taken > 0);
    mt_answers_free(answers);
    mt_clear();
    require("lift the bound", mt_limit(m, (mt_limits){0}));

    mt_stats before = mt_stats_now(m);
    check_int("a finite question", mt_one_int(mt_eval(m, E("+", 20, 22))), 42);
    mt_stats spent = mt_stats_since(before, mt_stats_now(m));
    check("is priced in inferences", spent.inferences > 0);
    return done(m);
}
