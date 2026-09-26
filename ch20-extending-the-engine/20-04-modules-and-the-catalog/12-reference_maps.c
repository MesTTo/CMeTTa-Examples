/* Purpose: a reference row's map decides which of a home's public heads
 *   arrive and under what names. C builds each map as the term it is and
 *   each name it expects as C strings: only and except select, prefix and
 *   qualified prepend a string, rename maps one head to another name or to a
 *   call pattern, and a lambda map answers any number of names. Every
 *   arriving map-value answers what C's model of the fixture's body
 *   computes, one more than its argument; a head no map admitted answers
 *   itself. A row with no map takes the space's default, set by pragma.
 * Guarantees: all twelve claims of the original hold
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
    assert(mt_one_int(mt_eval(m, E(name, x))) == x + MAP_OFFSET && name);
}

static const char *joined(char *buffer, size_t size, const char *prefix, const char *name)
{
    snprintf(buffer, size, "%s%s", prefix, name);
    return buffer;
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    char name[64];
    refer(m, E("only", E("map-value")));
    maps_value(m, "map-value", 2);
    assert(answers_are(mt_eval(m, E("map-label")), E(E("map-label"))) && "only leaves the rest out");
    refer(m, E("except", E("map-value")));
    assert(answers_are(mt_eval(m, E("map-label")), E("label")) && "except takes the rest");

    refer(m, E("prefix", "p."));
    refer(m, E("rename", E(E("map-value", "renamed"))));
    refer(m, E("qualified", "module"));
    maps_value(m, joined(name, sizeof name, "p.", "map-value"), 2);
    maps_value(m, "renamed", 2);
    maps_value(m, joined(name, sizeof name, "module.", "map-value"), 2);
    assert(answers_are(mt_eval(m, E("map-label")), E("label")) && "the label arrived once");

    refer(m, E("rename", E(E("map-value", E("only-two", 2)))));
    maps_value(m, "only-two", 2);

    static const char *const lambdas[] = { "lambda-one", "lambda-two" };
    refer(m, E("|->", E(V("h")), E("if", E("==", V("h"), "map-value"), E("superpose", E(lambdas[0], lambdas[1])), E("empty"))));
    for (size_t i = 0; i < 2; i++) maps_value(m, lambdas[i], 2);
    assert(answers_are(mt_eval(m, E(E("prefix", "text."), "word")), E(S(joined(name, sizeof name, "text.", "word"))))
           && "a map is an ordinary function");

    pragma(m, "from-map", E("prefix", "default."));
    refer(m, NULL);
    maps_value(m, joined(name, sizeof name, "default.", "map-value"), 2);
    pragma(m, "from-map", S("none"));
    mt_close(m);
    return 0;
}
