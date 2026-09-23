/* Purpose: set a startup stack budget and rewrite live catalog settings.
 * Owns resources: closes the configured process runtime.
 * Decides: this small host reserves a 256 MiB SWI stack ceiling.
 * Guarantees: runtime limits and catalog values are observable [tested: make check; commit=WORKTREE].
 * Open Obligations: None.
 */
#include "common.h"
int main(void)
{
    mt_config config = {.stack_limit=256u*1024u*1024u};
    metta *m = mt_open(&config); check("configured startup", m != NULL);
    check_answers("configured engine works", mt_eval(m, mt_expr("+", 20, 22)), "42");
    check("runtime inference budget", mt_limit(m, (mt_limits){.inferences=1000000}));
    check("budget is inspectable", mt_limits_of(m).inferences == 1000000);
    check("clear per-query budget", mt_limit(m, (mt_limits){0}));
    check("live presentation setting", mt_add(mt_catalog(m), mt_parse("(limit display-rows 7)")));
    check_answers("setting is language data", mt_run(m, "!(match &metta (limit display-rows $n) $n)"), "7");
    check("remove prior singleton value", mt_del(mt_catalog(m), mt_parse("(limit display-rows 7)")));
    check("rewrite presentation setting", mt_add(mt_catalog(m), mt_parse("(limit display-rows 3)")));
    check_answers("updated setting", mt_run(m, "!(match &metta (limit display-rows $n) $n)"), "3");
    return done(m, "runtime_configuration");
}
