/* Purpose: the carrier vocabulary and the algebra rows that use its words
 *   agree. The semiring row is exactly the words vocabularies.h generated
 *   from it, in its order, so a C enum names every carrier the catalog
 *   ships. budget's algebra row starts from infinity and is owned globally,
 *   budget and tropical both read ascending, and amplitude's zero is the
 *   complex zero; C reads each field it names through the row's own
 *   variables and holds it to the value C expects.
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

/* Whether an atom is want up to renaming variables; takes both, and shows
   them when they differ. */
static inline bool atom_is(mt_atom *got, mt_atom *want)
{
    bool holds = got && want && mt_alpha_eq(got, want);
    if (!holds) fprintf(stderr, "  got %s\n  want %s\n", got ? mt_show(got) : "nothing",
                        want ? mt_show(want) : "nothing");
    mt_drop(got);
    mt_drop(want);
    return holds;
}

/* (algebra carrier $combine $extend $zero $one $laws $carrier $requires $owner) */
static mt_atom *algebra_pattern(const char *carrier)
{
    return E("algebra", carrier, V("combine"), V("extend"), V("zero"), V("one"), V("laws"), V("carrier"), V("requires"),
             V("owner"));
}

/* One field of the one algebra row for CARRIER. */
static mt_atom *algebra_field(mt_space *catalog, const char *carrier, const char *field)
{
    mt_atom *value = NULL;
    size_t rows = 0;
    mt_rows (row, mt_match(catalog, algebra_pattern(carrier))) {
        if (!value) value = mt_keep(mt_bound(row, field));
        rows++;
    }
    require("one algebra row", rows == 1 && value != NULL);
    return value;
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    mt_space *catalog = mt_catalog(m);
    size_t n = MT_VOCABULARY_COUNT(mt_semiring_names);
    mt_atom **items = malloc((n + 2) * sizeof *items);
    require("room", items != NULL);
    items[0] = S("vocabulary");
    items[1] = S("semiring");
    for (size_t i = 0; i < n; i++) items[i + 2] = S(mt_semiring_names[i]);
    mt_atom *semiring = mt_exprv(n + 2, items);
    free(items);
    assert(answers_are(mt_match(catalog, mt_keep(semiring)), E(semiring)) && "the semiring row is the generated vocabulary");

    mt_atom *zero = algebra_field(catalog, "budget", "zero"), *owner = algebra_field(catalog, "budget", "owner");
    assert(atom_is(E(zero, owner), E("infinity", "global")) && "budget starts from infinity, owned globally");
    static const char *const ascending[] = { "budget", "tropical" };
    for (size_t i = 0; i < 2; i++)
        assert(answers_are(mt_match(catalog, E("claim", "semiring", ascending[i], V("p"), V("dir"))), E(E("claim", "semiring", ascending[i], "ordered", "ascending")))
               && "reads cheapest first");
    assert(atom_is(algebra_field(catalog, "amplitude", "zero"), E("complex", 0, 0)) && "amplitude's zero is the complex zero");
    mt_close(m);
    return 0;
}
