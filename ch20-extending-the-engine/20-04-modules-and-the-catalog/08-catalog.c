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
 * Guarantees: all six claims of the original hold
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
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    mt_space *catalog = mt_catalog(m);

    mt_atom *fidelity = vocabulary_row("fidelity", (const char *const *)mt_fidelity_names,
                                       MT_VOCABULARY_COUNT(mt_fidelity_names));
    assert(answers_are(mt_match(catalog, mt_keep(fidelity)), E(fidelity)) && "the fidelity row is the generated vocabulary");

    mt_list claims = { NULL, 0 };
    mt_rows (row, mt_match(catalog, E("kind", "handles", V("ctx"), V("entry"), V("claim"), V("det")))) {
        mt_atom **grown = mt_resize(claims.items, (claims.len + 1) * sizeof *grown);
        require("room", grown != NULL);
        claims.items = grown;
        claims.items[claims.len++] = mt_keep(mt_bound(row, "claim"));
    }
    assert(list_is(claims, E(E("one-of", "fidelity"))) && "the handles kind names that vocabulary for its claim");

    for (size_t i = 0; i < sizeof ordered / sizeof *ordered; i++)
        assert(answers_are(mt_match(catalog, E("claim", "semiring", ordered[i].semiring, V("p"), V("dir"))), E(E("claim", "semiring", ordered[i].semiring, "ordered", ordered[i].direction)))
               && "orderedness carries its direction");

    require("a third-party vocabulary",
            mt_add(catalog, vocabulary_row("freshness-level", freshness_levels,
                                           sizeof freshness_levels / sizeof *freshness_levels)));
    require("its shape", mt_add(catalog, E("kind", "freshness", "symbol", "pattern", E("one-of", "freshness-level"))));
    const char *declared = freshness_levels[1];
    require("one declaration", mt_add(catalog, E("freshness", S("&rows"), E("edge", V("a"), V("b")), declared)));
    assert(answers_are(mt_match(catalog, E("freshness", S("&rows"), V("shape"), V("level"))), E(E("freshness", S("&rows"), E("edge", V("a"), V("b")), declared)))
           && "a match finds the level C declared");
    require("route it by shape", mt_add(catalog, E("routed-by-shape", "freshness")));
    assert(answers_are(mt_match(catalog, E("routed-by-shape", "freshness")), E(E("routed-by-shape", "freshness"))) && "the routing row is there");
    mt_close(m);
    return 0;
}
