/* Purpose: a nondeterministic write. The original stores one (set key value)
 *   atom per alternative of a superposition; C holds the groups as a table,
 *   writes the eight atoms as one mt_add_all batch, which is one engine call
 *   as the original's one form is, and reads them back by name in the order
 *   they were written.
 * Guarantees: the original's claim holds [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

static const struct { int64_t key; const char *values[3]; size_t n; } GROUPS[] = {
    { 1, { "a", "b", "c" }, 3 }, { 2, { "d", "e", "f" }, 3 }, { 3, { "a", "b" }, 2 },
};
enum { GROUP_COUNT = sizeof GROUPS / sizeof *GROUPS };

int main(void)
{
    metta *m = open_engine();

    mt_list batch = { mt_alloc(8 * sizeof *batch.items), 0 };
    require("room for the batch", batch.items != NULL);
    for (size_t g = 0; g < GROUP_COUNT; g++)
        for (size_t v = 0; v < GROUPS[g].n; v++)
            batch.items[batch.len++] = E("set", GROUPS[g].key, GROUPS[g].values[v]);
    require("write the eight atoms in one call", mt_add_all(m, batch));

    size_t rows = 0, matched = 0, g = 0, v = 0;
    mt_rows (row, mt_match(m, E("set", V("x"), V("y")))) {
        matched += g < GROUP_COUNT && mt_int(mt_bound(row, "x")) == GROUPS[g].key &&
                   strcmp(mt_name(mt_bound(row, "y")), GROUPS[g].values[v]) == 0;
        rows++;
        if (g < GROUP_COUNT && ++v == GROUPS[g].n) g++, v = 0;
    }
    check("the eight facts come back in the order written", rows == 8 && matched == 8);
    return done(m);
}
