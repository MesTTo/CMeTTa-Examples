/* Purpose: Bound a divergent query and read engine counters.
 * Owns resources: local C handles are released before exit; a failed check
 *   terminates the example process.
 * Guarantees: the assertions below hold [tested: make check; commit=6022c3f48b6dc64752c6e49cfe9d985c7ac7a4e9].
 * Open Obligations: None.
 */
#include "common.h"
int main(void)
{
    metta *m = open_engine();
    check("define unbounded source", mt_do(m, "(= (from $n) (superpose ($n (from (+ $n 1)))))"));
    check("set inference bound", mt_limit(m, (mt_limits){.inferences = 5000}));
    mt_answers *answers = mt_eval(m, mt_expr("from", 0));
    check("open bounded cursor", answers != NULL);
    const mt_atom *answer;
    mt_status status;
    while ((status = mt_step(answers, &answer)) == MT_ROW) {}
    check("limit differs from exhaustion", status == MT_LIMIT && mt_error() == MT_LIMIT);
    mt_answers_free(answers); mt_clear();
    check("clear bound", mt_limit(m, (mt_limits){0}));
    mt_stats before = mt_stats_now(m);
    check_answers("finite evaluation", mt_run(m, "!(+ 20 22)"), "42");
    mt_stats spent = mt_stats_since(before, mt_stats_now(m));
    check("engine work counted", spent.inferences > 0);
    return done(m, "engine_controls");
}
