/* Purpose: when a referenced home loads. Under the lazy policy the row
 *   publishes the home's manifest and an equation compiles when first
 *   called; under background a worker loads it and a call waits until the
 *   home is ready. Every arriving function answers what C's model of its
 *   fixture computes, one more than its argument. Both deferred policies
 *   refuse a home whose initializer has an effect before it runs, and the
 *   refusal reaches C as the door's MT_ERROR naming the println! form and
 *   the eager remedy.
 * Guarantees: all five claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include "modules.h"

/* Divergence dc6887a5e38833a7: the original names its fixture relative to its
   own file, ./_fixtures/references/...; a C program has no importing
   file, so it names the same fixture from the engine tree where the twin
   runs, and each (from ...) row it stores spells that path. */
#define REFERENCES "./" MODULES_FIXTURES "references/"
enum { OFFSET = 1 };

static void policy(metta *m, const char *load) { pragma(m, "load", S(load)); }

int main(void)
{
    metta *m = open_engine();
    policy(m, "lazy");
    require("a lazy reference", mt_add(m, E("from", S(REFERENCES "maps"), E("prefix", "lazy."))));
    const int64_t x = 41;
    check_int("an equation compiles when first called", mt_one_int(mt_eval(m, E("lazy.map-value", x))), x + OFFSET);
    check_answers("and another head arrives", mt_eval(m, E("lazy.map-label")), "label");

    policy(m, "background");
    require("a background reference", mt_add(m, E("from", S(REFERENCES "background"))));
    check_int("a call waits for its home", mt_one_int(mt_eval(m, E("background-value", x))), x + OFFSET);

    static const char *const deferred[] = { "background", "lazy" };
    for (size_t i = 0; i < 2; i++) {
        policy(m, deferred[i]);
        mt_clear();
        bool added = mt_add(m, E("from", S(REFERENCES "effectful")));
        check("an effectful initializer is refused before it runs",
              !added && mt_error() == MT_ERROR && mt_errmsg() && strstr(mt_errmsg(), "println!") &&
                  strstr(mt_errmsg(), "eager"));
        mt_clear();
    }
    policy(m, "eager");
    return done(m);
}
