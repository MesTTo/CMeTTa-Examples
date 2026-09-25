/* Purpose: ground expressions as space names, each parameter set a space of
 *   its own. C keeps the two cache instances as a table of base, limit and
 *   entry, and opens each with mt_space_of, which declares a parametric name
 *   through the engine's new-space. One equation, added to both, reads
 *   the parameters of whichever instance holds it by destructuring
 *   context-space, so each answers its own row of the table, as does each
 *   instance's entry, and the engine types a parametric name as a space.
 *   A parameter is a symbol: &primary-kb names no space here.
 * Guarantees: all five claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

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
    metta *m = open_engine();
    mt_space *spaces[CACHES];
    for (size_t i = 0; i < CACHES; i++) {
        spaces[i] = mt_space_of(m, name_of(&caches[i]));
        require("open the instance", spaces[i] != NULL);
        require("its cache-config", mt_add(spaces[i], E("=", E("cache-config"),
                                                      E("let", E("cache", V("base"), V("limit")), E("context-space"), E("config", V("base"), V("limit"))))));
        require("its entry", mt_add(spaces[i], E("entry", caches[i].entry)));
    }
    for (size_t i = 0; i < CACHES; i++)
        check_answers("the equation reads its own instance's parameters", mt_eval(spaces[i], E("cache-config")),
                      E("config", S(caches[i].base), caches[i].limit));
    for (size_t i = 0; i < CACHES; i++)
        check_answers("each instance holds its own entry", mt_match(spaces[i], E("entry", V("which"))),
                      E("entry", caches[i].entry));
    check_answers("a parametric name is typed as a space", mt_eval(m, E("get-type", name_of(&caches[0]))), S("SpaceType"));
    for (size_t i = 0; i < CACHES; i++) mt_space_close(spaces[i]);
    return done(m);
}
