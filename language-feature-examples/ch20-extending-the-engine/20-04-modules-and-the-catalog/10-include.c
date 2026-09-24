/* Purpose: include pastes a module's forms into the current space and
 *   answers its last directive's answer, where import! merges a module and
 *   answers True. The rows fixture defines included-double as twice its
 *   argument and ends by asking it for 21, so C expects twice 21, then its
 *   function and its fact in this space; a module with no directive answers
 *   nothing but still lands its fact. A name nothing resolves to is refused
 *   by name, and the two module-path bases self and top are refused as not
 *   modules; C builds each refusal it expects.
 * Guarantees: all nine claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include "modules.h"

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
    check_answers("include refuses by name", mt_eval(m, E("catch", E("include", mt_keep(name)))),
                  E("Error", E("include", name), T(why)));
}

int main(void)
{
    metta *m = open_engine();
    check_int("include answers its last directive", mt_one_int(mt_eval(m, E("include", included("included/rows")))),
              INCLUDED_FACTOR * LAST_ASKED);
    const int64_t x = 5;
    check_int("its function is in this space", mt_one_int(mt_eval(m, E("included-double", x))), INCLUDED_FACTOR * x);
    check_answers("and its fact", mt_match(m, E("included-fact", V("x"))), E("included-fact", "carried"));
    check_answers("import! answers True", mt_eval(m, E("import!", "&self", included("included/rows"))), B(true));
    check_none("a module with no directive answers nothing", mt_eval(m, E("include", included("included/silent"))));
    check_answers("but its fact lands", mt_match(m, E("silent-fact", V("x"))), E("silent-fact", "here"));

    static const char *const missing = "nosuchmodule";
    char why[128];
    snprintf(why, sizeof why, "no module named %s is available", missing);
    refused(m, S(missing), why);
    static const char *const bases[] = { "self", "top" };
    for (size_t i = 0; i < 2; i++) refused(m, S(bases[i]), "include: the running context is not a module");
    return done(m);
}
