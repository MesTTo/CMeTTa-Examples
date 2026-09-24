/* Purpose: the carrier vocabulary and the algebra rows that use its words
 *   agree. The semiring row is exactly the words vocabularies.h generated
 *   from it, in its order, so a C enum names every carrier the catalog
 *   ships. budget's algebra row starts from infinity and is owned globally,
 *   budget and tropical both read ascending, and amplitude's zero is the
 *   complex zero; C reads each field it names through the row's own
 *   variables and holds it to the value C expects.
 * Guarantees: all five claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

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
    metta *m = open_engine();
    mt_space *catalog = mt_catalog(m);
    size_t n = MT_VOCABULARY_COUNT(mt_semiring_names);
    mt_atom **items = malloc((n + 2) * sizeof *items);
    require("room", items != NULL);
    items[0] = S("vocabulary");
    items[1] = S("semiring");
    for (size_t i = 0; i < n; i++) items[i + 2] = S(mt_semiring_names[i]);
    mt_atom *semiring = mt_exprv(n + 2, items);
    free(items);
    check_answers("the semiring row is the generated vocabulary", mt_match(catalog, mt_keep(semiring)), semiring);

    mt_atom *zero = algebra_field(catalog, "budget", "zero"), *owner = algebra_field(catalog, "budget", "owner");
    check_atom("budget starts from infinity, owned globally", E(zero, owner), E("infinity", "global"));
    static const char *const ascending[] = { "budget", "tropical" };
    for (size_t i = 0; i < 2; i++)
        check_answers("reads cheapest first", mt_match(catalog, E("claim", "semiring", ascending[i], V("p"), V("dir"))),
                      E("claim", "semiring", ascending[i], "ordered", "ascending"));
    check_atom("amplitude's zero is the complex zero", algebra_field(catalog, "amplitude", "zero"), E("complex", 0, 0));
    return done(m);
}
