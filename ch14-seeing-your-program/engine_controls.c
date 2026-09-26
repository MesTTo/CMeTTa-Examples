/* Purpose: bound and measure. An endless generator is stopped by an inference
 *   budget, which a cursor reports as MT_LIMIT rather than as exhaustion or a
 *   fault, and two samples of the engine's counters price a finite question.
 * Guarantees: the bound stops the stream with MT_LIMIT, and a measured
 *   evaluation spends inferences [tested 2026-09-27T00:35:58+10:00:
 *   make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>

/* A door the program needs before it can go on: on refusal, say why and stop. */
#define require(what, ok) \
    ((ok) ? (void)0 : (fprintf(stderr, "%s: %s\n", (what), mt_errmsg()), exit(EXIT_FAILURE)))

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
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
    assert(status == MT_LIMIT && mt_error() == MT_LIMIT && "the bound stops it, and says so");
    assert(taken > 0 && "after some answers");
    mt_answers_free(answers);
    mt_clear();
    require("lift the bound", mt_limit(m, (mt_limits){0}));

    mt_stats before = mt_stats_now(m);
    assert(mt_one_int(mt_eval(m, E("+", 20, 22))) == 42 && "a finite question");
    mt_stats spent = mt_stats_since(before, mt_stats_now(m));
    assert(spent.inferences > 0 && "is priced in inferences");
    mt_close(m);
    return 0;
}
