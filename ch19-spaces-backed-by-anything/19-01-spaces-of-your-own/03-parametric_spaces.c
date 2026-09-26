/* Purpose: ground expressions as space names, each parameter set a space of
 *   its own. C keeps the two cache instances as a table of base, limit and
 *   entry, and opens each with mt_space_of, which declares a parametric name
 *   through the engine's new-space. One equation, added to both, reads
 *   the parameters of whichever instance holds it by destructuring
 *   context-space, so each answers its own row of the table, as does each
 *   instance's entry, and the engine types a parametric name as a space.
 *   A parameter is a symbol: &primary-kb names no space here.
 * Guarantees: all five claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>

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

typedef struct cache {
    const char *base;
    int64_t limit;
    const char *entry;
} cache;

static const cache caches[] = { { "&primary-kb", 100, "primary" }, { "&secondary-kb", 10, "secondary" } };
#define CACHES (sizeof caches / sizeof *caches)

static mt_atom *name_of(const cache *c) { return E("cache", S(c->base), c->limit); }

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    mt_space *spaces[CACHES];
    for (size_t i = 0; i < CACHES; i++) {
        spaces[i] = mt_space_of(m, name_of(&caches[i]));
        require("open the instance", spaces[i] != NULL);
        require("its cache-config", mt_add(spaces[i], E("=", E("cache-config"),
                                                      E("let", E("cache", V("base"), V("limit")), E("context-space"), E("config", V("base"), V("limit"))))));
        require("its entry", mt_add(spaces[i], E("entry", caches[i].entry)));
    }
    for (size_t i = 0; i < CACHES; i++)
        assert(answers_are(mt_eval(spaces[i], E("cache-config")), E(E("config", S(caches[i].base), caches[i].limit)))
               && "the equation reads its own instance's parameters");
    for (size_t i = 0; i < CACHES; i++)
        assert(answers_are(mt_match(spaces[i], E("entry", V("which"))), E(E("entry", caches[i].entry)))
               && "each instance holds its own entry");
    assert(answers_are(mt_eval(m, E("get-type", name_of(&caches[0]))), E(S("SpaceType"))) && "a parametric name is typed as a space");
    for (size_t i = 0; i < CACHES; i++) mt_space_close(spaces[i]);
    mt_close(m);
    return 0;
}
