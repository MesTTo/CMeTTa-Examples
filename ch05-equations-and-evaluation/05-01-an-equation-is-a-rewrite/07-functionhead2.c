/* Purpose: a relational constraint, chained. The animals' properties are a
 *   C table turned into (= (property animal) True) equations; animal holds
 *   of what is living and a being, cat of an animal that is also small, and
 *   small answers nothing, not a residual call, where it has no equation.
 *   The answers are sorted with qsort in the engine's order.
 * Guarantees: (cat $X) finds cat42 and garfield
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

static const struct { const char *animal; const char *properties[3]; } animals[] = {
    { "garfield", { "living", "being", "small" } },
    { "snoopy",   { "living", "being", NULL } },
    { "roomba",   { "being", "small", NULL } },
    { "cat42",    { "living", "being", "small" } },
};

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
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
    assert(list_is(cats, E("cat42", "garfield")) && "the cats are the small living beings");
    mt_close(m);
    return 0;
}
