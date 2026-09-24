/* Purpose: a relational constraint, chained. The animals' properties are a
 *   C table turned into (= (property animal) True) equations; animal holds
 *   of what is living and a being, cat of an animal that is also small, and
 *   small answers nothing, not a residual call, where it has no equation.
 *   The answers are sorted with qsort in the engine's order.
 * Guarantees: (cat $X) finds cat42 and garfield [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

static const struct { const char *animal; const char *properties[3]; } animals[] = {
    { "garfield", { "living", "being", "small" } },
    { "snoopy",   { "living", "being", NULL } },
    { "roomba",   { "being", "small", NULL } },
    { "cat42",    { "living", "being", "small" } },
};

int main(void)
{
    metta *m = open_engine();
    require("small answers nothing where it has no equation",
            mt_add(mt_catalog(m), E("dispatch-policy", "small", "NoMatchEnum", "NoMatchFail")));
    for (size_t a = 0; a < sizeof animals / sizeof animals[0]; a++)
        for (size_t p = 0; p < 3 && animals[a].properties[p]; p++)
            require("a property", mt_add(m, E("=", E(animals[a].properties[p], animals[a].animal), B(true))));

    /* (= (only $C $X) (let $constraint $C $X)) */
    require("define only", mt_add(m, E("=", E("only", V("C"), V("X")), E("let", V("constraint"), V("C"), V("X")))));
    /* (= (animal $X) (only ((living $X) (being $X)) $X)) */
    require("define animal", mt_add(m, E("=", E("animal", V("X")),
                                         E("only", E(E("living", V("X")), E("being", V("X"))), V("X")))));
    /* (= (cat $A) (let $A (animal $X) (only (small $X) $X))) */
    require("define cat", mt_add(m, E("=", E("cat", V("A")),
                                      E("let", V("A"), E("animal", V("X")), E("only", E("small", V("X")), V("X"))))));

    mt_list cats = mt_all(mt_eval(m, E("cat", V("X"))));
    qsort(cats.items, cats.len, sizeof *cats.items, mt_order);
    check_list("the cats are the small living beings", cats, "cat42", "garfield");
    return done(m);
}
