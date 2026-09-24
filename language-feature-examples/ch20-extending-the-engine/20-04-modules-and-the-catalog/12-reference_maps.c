/* Purpose: a reference row's map decides which of a home's public heads
 *   arrive and under what names. C builds each map as the term it is and
 *   each name it expects as C strings: only and except select, prefix and
 *   qualified prepend a string, rename maps one head to another name or to a
 *   call pattern, and a lambda map answers any number of names. Every
 *   arriving map-value answers what C's model of the fixture's body
 *   computes, one more than its argument; a head no map admitted answers
 *   itself. A row with no map takes the space's default, set by pragma.
 * Guarantees: all twelve claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"
#include "modules.h"

/* Divergence 9174f80e4789e79a: the original names its fixture relative to its
   own file, ./_fixtures/references/...; a C program has no importing
   file, so it names the same fixture from the engine tree where the twin
   runs, and each (from ...) row it stores spells that path. */
#define MAPS "./" MODULES_FIXTURES "references/maps"
enum { MAP_OFFSET = 1 };

static void refer(metta *m, mt_atom *map)
{
    require("a reference row", mt_add(m, map ? E("from", S(MAPS), map) : E("from", S(MAPS))));
}

/* NAME, which must answer one more than X. */
static void maps_value(metta *m, const char *name, int64_t x)
{
    check_int(name, mt_one_int(mt_eval(m, E(name, x))), x + MAP_OFFSET);
}

static const char *joined(char *buffer, size_t size, const char *prefix, const char *name)
{
    snprintf(buffer, size, "%s%s", prefix, name);
    return buffer;
}

int main(void)
{
    metta *m = open_engine();
    char name[64];
    refer(m, E("only", E("map-value")));
    maps_value(m, "map-value", 2);
    check_answers("only leaves the rest out", mt_eval(m, E("map-label")), E("map-label"));
    refer(m, E("except", E("map-value")));
    check_answers("except takes the rest", mt_eval(m, E("map-label")), "label");

    refer(m, E("prefix", "p."));
    refer(m, E("rename", E(E("map-value", "renamed"))));
    refer(m, E("qualified", "module"));
    maps_value(m, joined(name, sizeof name, "p.", "map-value"), 2);
    maps_value(m, "renamed", 2);
    maps_value(m, joined(name, sizeof name, "module.", "map-value"), 2);
    check_answers("the label arrived once", mt_eval(m, E("map-label")), "label");

    refer(m, E("rename", E(E("map-value", E("only-two", 2)))));
    maps_value(m, "only-two", 2);

    static const char *const lambdas[] = { "lambda-one", "lambda-two" };
    refer(m, E("|->", E(V("h")), E("if", E("==", V("h"), "map-value"), E("superpose", E(lambdas[0], lambdas[1])), E("empty"))));
    for (size_t i = 0; i < 2; i++) maps_value(m, lambdas[i], 2);
    check_answers("a map is an ordinary function", mt_eval(m, E(E("prefix", "text."), "word")),
                  S(joined(name, sizeof name, "text.", "word")));

    pragma(m, "from-map", E("prefix", "default."));
    refer(m, NULL);
    maps_value(m, joined(name, sizeof name, "default.", "map-value"), 2);
    pragma(m, "from-map", S("none"));
    return done(m);
}
