/* Purpose: a C value that decides its own matches. A POSIX regular
 *   expression compiled in C becomes a matcher atom; wherever the engine
 *   unifies it with a symbol, the C callback runs regexec() and the match
 *   succeeds or fails on its answer.
 * Owns resources: the compiled regex, freed with the matcher.
 * Guarantees: ^a matches abbey and refuses zebra
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <regex.h>

/* A door the program needs before it can go on: on refusal, say why and stop. */
#define require(what, ok) \
    ((ok) ? (void)0 : (fprintf(stderr, "%s: %s\n", (what), mt_errmsg()), exit(EXIT_FAILURE)))

/* Whether a list holds exactly the children of want, in order, each equal
   up to renaming variables; takes both, and shows them when they differ. */
static inline bool list_is(mt_list got, mt_atom *want)
{
    bool holds = mt_ok() && want && got.len == mt_len(want);
    for (size_t i = 0; holds && i < got.len; i++)
        holds = mt_alpha_eq(got.items[i], mt_at(want, i));
    if (!holds) {
        fprintf(stderr, "  got");
        for (size_t i = 0; i < got.len; i++) fprintf(stderr, " %s", mt_show(got.items[i]));
        fprintf(stderr, "\n  want %s\n", want ? mt_show(want) : "nothing");
    }
    mt_list_free(got);
    mt_drop(want);
    return holds;
}

/* Whether a query answers exactly the children of want; takes both. */
static inline bool answers_are(mt_answers *answers, mt_atom *want)
{
    return list_is(mt_all(answers), want);
}

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
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    regex_t *starts_with_a = malloc(sizeof *starts_with_a);
    require("allocate the regex", starts_with_a != NULL);
    require("compile ^a", regcomp(starts_with_a, "^a", REG_EXTENDED | REG_NOSUB) == 0);
    mt_atom *matcher = mt_matcher(matches, starts_with_a, release);
    require("make it a matcher", matcher != NULL);

    assert(answers_are(mt_eval(m, E("unify", mt_keep(matcher), "abbey", "hit", "miss")), E("hit")) && "abbey matches");
    assert(answers_are(mt_eval(m, E("unify", mt_keep(matcher), "zebra", "hit", "miss")), E("miss")) && "zebra does not");
    require("release the matcher", mt_object_free(matcher));
    mt_close(m);
    return 0;
}
