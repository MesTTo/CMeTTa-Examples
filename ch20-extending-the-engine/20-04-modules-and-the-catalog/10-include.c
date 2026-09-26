/* Purpose: include pastes a module's forms into the current space and
 *   answers its last directive's answer, where import! merges a module and
 *   answers True. The rows fixture defines included-double as twice its
 *   argument and ends by asking it for 21, so C expects twice 21, then its
 *   function and its fact in this space; a module with no directive answers
 *   nothing but still lands its fact. A name nothing resolves to is refused
 *   by name, and the two module-path bases self and top are refused as not
 *   modules; C builds each refusal it expects.
 * Guarantees: all nine claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include "_fixtures/modules.h"

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

enum { INCLUDED_FACTOR = 2, LAST_ASKED = 21 };

static mt_atom *included(const char *fixture)
{
    char path[512];
    snprintf(path, sizeof path, "%s%s", MODULES_FIXTURES, fixture);
    return S(path);
}

/* The refusal (catch (include name)) answers. TAKES name. */
static void refused(metta *m, mt_atom *name, const char *why)
{
    assert(answers_are(mt_eval(m, E("catch", E("include", mt_keep(name)))), E(E("Error", E("include", name), T(why))))
           && "include refuses by name");
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    assert(mt_one_int(mt_eval(m, E("include", included("included/rows")))) == INCLUDED_FACTOR * LAST_ASKED
           && "include answers its last directive");
    const int64_t x = 5;
    assert(mt_one_int(mt_eval(m, E("included-double", x))) == INCLUDED_FACTOR * x && "its function is in this space");
    assert(answers_are(mt_match(m, E("included-fact", V("x"))), E(E("included-fact", "carried"))) && "and its fact");
    assert(answers_are(mt_eval(m, E("import!", "&self", included("included/rows"))), E(B(true))) && "import! answers True");
    assert(!mt_first(mt_eval(m, E("include", included("included/silent")))) && mt_ok() && "a module with no directive answers nothing");
    assert(answers_are(mt_match(m, E("silent-fact", V("x"))), E(E("silent-fact", "here"))) && "but its fact lands");

    static const char *const missing = "nosuchmodule";
    char why[128];
    snprintf(why, sizeof why, "no module named %s is available", missing);
    refused(m, S(missing), why);
    static const char *const bases[] = { "self", "top" };
    for (size_t i = 0; i < 2; i++) refused(m, S(bases[i]), "include: the running context is not a module");
    mt_close(m);
    return 0;
}
