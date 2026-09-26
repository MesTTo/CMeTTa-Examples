/* Purpose: mt_all() is a snapshot. The collected list is the program's own,
 *   so a write after collecting changes the space and not the list.
 * Guarantees: the snapshot keeps one row while the space grows to two
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

/* The same question as a truth value, for a condition with more in it:
   borrows the atom the program holds and takes the expectation. */
static inline bool alpha_equal(const mt_atom *got, mt_atom *want)
{
    bool equal = got && want && mt_alpha_eq(got, want);
    mt_drop(want);
    return equal;
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("store (item 1)", mt_add(m, E("item", 1)));
    mt_list snapshot = mt_all(mt_atoms(m));
    require("store (item 2)", mt_add(m, E("item", 2)));

    assert((int64_t)snapshot.len == 1 && "the snapshot still holds one row");
    assert(snapshot.len == 1 && alpha_equal(snapshot.items[0], E("item", 1)) && "and it is (item 1)");
    assert((int64_t)mt_count(m) == 2 && "while the space holds two");
    mt_list_free(snapshot);
    mt_close(m);
    return 0;
}
