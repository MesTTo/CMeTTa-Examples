/* Purpose: Let a POSIX regular-expression value own engine unification.
 * Owns resources: local handles and host storage are released before success;
 *   a failed assertion terminates this example process.
 * Guarantees: results are asserted [tested: make check; commit=6022c3f48b6dc64752c6e49cfe9d985c7ac7a4e9].
 * Open Obligations: None.
 */
#include "common.h"
#include <regex.h>
static mt_status matches(mt_call *call, void *user)
{
    regex_t *regex = user;
    const char *name = mt_name(mt_arg(call, 0));
    if (!name) return mt_fail(call, "regex expects a symbol or text");
    int result = regexec(regex, name, 0, NULL, 0);
    if (result != 0 && result != REG_NOMATCH) return mt_fail(call, "regex execution failed");
    return result == 0 ? mt_answer(call, mt_keep(mt_arg(call, 0))) : MT_FAIL;
}
static void release(void *user) { regfree(user); free(user); }
int main(void)
{
    metta *m = open_engine(); regex_t *regex = malloc(sizeof(*regex));
    check("allocate regex", regex != NULL);
    check("compile POSIX regex", regcomp(regex, "^a", REG_EXTENDED | REG_NOSUB) == 0);
    mt_atom *matcher = mt_matcher(matches, regex, release); check("grounded matcher", matcher != NULL);
    check_answers("regex owns a positive match", mt_eval(m,
        mt_expr("unify", mt_keep(matcher), "abbey", "hit", "miss")), "hit");
    check_answers("regex owns a negative match", mt_eval(m,
        mt_expr("unify", mt_keep(matcher), "zebra", "hit", "miss")), "miss");
    check("release regex after engine aliases", mt_object_free(matcher));
    return done(m, "regex_matching");
}
