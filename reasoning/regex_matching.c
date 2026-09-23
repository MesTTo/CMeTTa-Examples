/* Purpose: a C value that decides its own matches. A POSIX regular
 *   expression compiled in C becomes a matcher atom; wherever the engine
 *   unifies it with a symbol, the C callback runs regexec() and the match
 *   succeeds or fails on its answer.
 * Owns resources: the compiled regex, freed with the matcher.
 * Guarantees: ^a matches abbey and refuses zebra [tested: make check;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"
#include <regex.h>

static mt_status matches(mt_call *call, void *user)
{
    const char *name = mt_name(mt_arg(call, 0));
    if (!name) return mt_fail(call, "a regex matches symbols and text");
    int result = regexec(user, name, 0, NULL, 0);
    if (result != 0 && result != REG_NOMATCH) return mt_fail(call, "regexec failed");
    return result == 0 ? mt_answer(call, mt_keep(mt_arg(call, 0))) : MT_FAIL;
}

static void release(void *regex)
{
    regfree(regex);
    free(regex);
}

int main(void)
{
    metta *m = open_engine();
    regex_t *starts_with_a = malloc(sizeof *starts_with_a);
    require("allocate the regex", starts_with_a != NULL);
    require("compile ^a", regcomp(starts_with_a, "^a", REG_EXTENDED | REG_NOSUB) == 0);
    mt_atom *matcher = mt_matcher(matches, starts_with_a, release);
    require("make it a matcher", matcher != NULL);

    check_answers("abbey matches", mt_eval(m, E("unify", mt_keep(matcher), "abbey", "hit", "miss")), "hit");
    check_answers("zebra does not", mt_eval(m, E("unify", mt_keep(matcher), "zebra", "hit", "miss")), "miss");
    require("release the matcher", mt_object_free(matcher));
    return done(m);
}
