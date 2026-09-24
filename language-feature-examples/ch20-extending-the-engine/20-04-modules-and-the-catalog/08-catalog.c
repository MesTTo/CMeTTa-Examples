/* Purpose: the catalog describes its own kinds, as data a C program reads.
 *   The fidelity vocabulary row is exactly the words vocabularies.h
 *   generated from it, in its order; the handles kind row names that
 *   vocabulary for its claim position; and orderedness is a claim row per
 *   semiring carrying its direction, which C holds to its own table of
 *   directions. A third-party kind is C data added to the catalog, its
 *   vocabulary built from C's table of levels, its shape and one
 *   declaration, and from then on the catalog guards it and a match finds
 *   the level C declared; declaring it routed by shape is one more row.
 *   &rows names no space here, so it is the symbol the original writes.
 * Guarantees: all six claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

/* (vocabulary name words...) from a generated table. */
static mt_atom *vocabulary_row(const char *name, const char *const *words, size_t n)
{
    mt_atom **items = malloc((n + 2) * sizeof *items);
    require("room", items != NULL);
    items[0] = S("vocabulary");
    items[1] = S(name);
    for (size_t i = 0; i < n; i++) items[i + 2] = S(words[i]);
    mt_atom *row = mt_exprv(n + 2, items);
    free(items);
    return row;
}

/* Which end a (top k ...) slice of each ordered semiring takes. */
static const struct { const char *semiring, *direction; } ordered[] = {
    { "ranked", "descending" }, { "tropical", "ascending" },
};

static const char *const freshness_levels[] = { "live", "cached", "stale" };

int main(void)
{
    metta *m = open_engine();
    mt_space *catalog = mt_catalog(m);

    mt_atom *fidelity = vocabulary_row("fidelity", (const char *const *)mt_fidelity_names,
                                       MT_VOCABULARY_COUNT(mt_fidelity_names));
    check_answers("the fidelity row is the generated vocabulary", mt_match(catalog, mt_keep(fidelity)), fidelity);

    mt_list claims = { NULL, 0 };
    mt_rows (row, mt_match(catalog, E("kind", "handles", V("ctx"), V("entry"), V("claim"), V("det")))) {
        mt_atom **grown = mt_resize(claims.items, (claims.len + 1) * sizeof *grown);
        require("room", grown != NULL);
        claims.items = grown;
        claims.items[claims.len++] = mt_keep(mt_bound(row, "claim"));
    }
    check_list("the handles kind names that vocabulary for its claim", claims, E("one-of", "fidelity"));

    for (size_t i = 0; i < sizeof ordered / sizeof *ordered; i++)
        check_answers("orderedness carries its direction",
                      mt_match(catalog, E("claim", "semiring", ordered[i].semiring, V("p"), V("dir"))),
                      E("claim", "semiring", ordered[i].semiring, "ordered", ordered[i].direction));

    require("a third-party vocabulary",
            mt_add(catalog, vocabulary_row("freshness-level", freshness_levels,
                                           sizeof freshness_levels / sizeof *freshness_levels)));
    require("its shape", mt_add(catalog, E("kind", "freshness", "symbol", "pattern", E("one-of", "freshness-level"))));
    const char *declared = freshness_levels[1];
    require("one declaration", mt_add(catalog, E("freshness", S("&rows"), E("edge", V("a"), V("b")), declared)));
    check_answers("a match finds the level C declared", mt_match(catalog, E("freshness", S("&rows"), V("shape"), V("level"))),
                  E("freshness", S("&rows"), E("edge", V("a"), V("b")), declared));
    require("route it by shape", mt_add(catalog, E("routed-by-shape", "freshness")));
    check_answers("the routing row is there", mt_match(catalog, E("routed-by-shape", "freshness")), E("routed-by-shape", "freshness"));
    return done(m);
}
