/* Purpose: when a referenced home loads. Under the lazy policy the row
 *   publishes the home's manifest and an equation compiles when first
 *   called; under background a worker loads it and a call waits until the
 *   home is ready. Every arriving function answers what C's model of its
 *   fixture computes, one more than its argument. Both deferred policies
 *   refuse a home whose initializer has an effect before it runs, and the
 *   refusal reaches C as the door's MT_ERROR naming the println! form and
 *   the eager remedy.
 * Guarantees: all five claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <string.h>
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

/* Divergence dc6887a5e38833a7: the original names its fixture relative to its
   own file, ./_fixtures/references/...; a C program has no importing
   file, so it names the same fixture from the engine tree where the twin
   runs, and each (from ...) row it stores spells that path. */
#define REFERENCES "./" MODULES_FIXTURES "references/"
enum { OFFSET = 1 };

static void policy(metta *m, const char *load) { pragma(m, "load", S(load)); }

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    policy(m, "lazy");
    require("a lazy reference", mt_add(m, E("from", S(REFERENCES "maps"), E("prefix", "lazy."))));
    const int64_t x = 41;
    assert(mt_one_int(mt_eval(m, E("lazy.map-value", x))) == x + OFFSET && "an equation compiles when first called");
    assert(answers_are(mt_eval(m, E("lazy.map-label")), E("label")) && "and another head arrives");

    policy(m, "background");
    require("a background reference", mt_add(m, E("from", S(REFERENCES "background"))));
    assert(mt_one_int(mt_eval(m, E("background-value", x))) == x + OFFSET && "a call waits for its home");

    static const char *const deferred[] = { "background", "lazy" };
    for (size_t i = 0; i < 2; i++) {
        policy(m, deferred[i]);
        mt_clear();
        bool added = mt_add(m, E("from", S(REFERENCES "effectful")));
        assert(!added && mt_error() == MT_ERROR && mt_errmsg() && strstr(mt_errmsg(), "println!") &&
                   strstr(mt_errmsg(), "eager")
               && "an effectful initializer is refused before it runs");
        mt_clear();
    }
    policy(m, "eager");
    mt_close(m);
    return 0;
}
