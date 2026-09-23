/* Purpose: the multiset operations, computed twice. unique-atom,
 *   union-atom, intersection-atom and subtraction-atom run in the engine,
 *   and C runs the same algebra over the expressions' children with mt_eq:
 *   first occurrences, concatenation, and copy-for-copy cancellation in the
 *   left side's order. Every engine answer must be C's.
 * Guarantees: all eight claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

enum { MOST = 16 };

static mt_atom *unique(const mt_atom *a)
{
    mt_atom *out[MOST];
    size_t n = 0;
    for (size_t i = 0; i < mt_len(a); i++) {
        bool seen = false;
        for (size_t j = 0; j < n && !seen; j++) seen = mt_eq(out[j], mt_at(a, i));
        if (!seen) out[n++] = mt_keep(mt_at(a, i));
    }
    return mt_exprv(n, out);
}

static mt_atom *union_of(const mt_atom *a, const mt_atom *b)
{
    mt_atom *out[MOST];
    size_t n = 0;
    for (size_t i = 0; i < mt_len(a); i++) out[n++] = mt_keep(mt_at(a, i));
    for (size_t i = 0; i < mt_len(b); i++) out[n++] = mt_keep(mt_at(b, i));
    return mt_exprv(n, out);
}

/* a's children in order, kept when (intersection) or unless (subtraction)
   b still holds an unused copy of them. */
static mt_atom *against(const mt_atom *a, const mt_atom *b, bool keep_shared)
{
    mt_atom *out[MOST];
    bool used[MOST] = { false };
    size_t n = 0;
    for (size_t i = 0; i < mt_len(a); i++) {
        bool found = false;
        for (size_t j = 0; j < mt_len(b) && !found; j++)
            if (!used[j] && mt_eq(mt_at(a, i), mt_at(b, j))) used[j] = found = true;
        if (found == keep_shared) out[n++] = mt_keep(mt_at(a, i));
    }
    return mt_exprv(n, out);
}

int main(void)
{
    metta *m = open_engine();
    mt_atom *repeated = E("a", "b", "c", "d", "d");
    mt_atom *left = E("a", "b", "b", "c"), *right = E("b", "c", "c", "d");
    mt_atom *wide = E("a", "b", "c", "c"), *wider = E("b", "c", "c", "c", "d"), *narrow = E("b", "c", "d");
    mt_atom *thrice = E("a", "a", "a"), *once = E("a"), *two = E("a", "b");

    check_answers("unique-atom", mt_eval(m, E("unique-atom", mt_keep(repeated))), unique(repeated));
    check_answers("union-atom", mt_eval(m, E("union-atom", mt_keep(left), mt_keep(right))), union_of(left, right));
    check_answers("intersection-atom", mt_eval(m, E("intersection-atom", mt_keep(wide), mt_keep(wider))),
                  against(wide, wider, true));
    check_answers("subtraction-atom", mt_eval(m, E("subtraction-atom", mt_keep(left), mt_keep(right))),
                  against(left, right, false));
    check_answers("intersection with fewer copies", mt_eval(m, E("intersection-atom", mt_keep(wide), mt_keep(narrow))),
                  against(wide, narrow, true));
    check_answers("one copy in common", mt_eval(m, E("intersection-atom", mt_keep(thrice), mt_keep(once))),
                  against(thrice, once, true));
    check_answers("one copy cancelled", mt_eval(m, E("subtraction-atom", mt_keep(thrice), mt_keep(once))),
                  against(thrice, once, false));
    mt_atom *none = mt_unit();
    check_answers("nothing in common with nothing", mt_eval(m, E("intersection-atom", mt_keep(two), mt_keep(none))),
                  against(two, none, true));
    mt_atom *all[] = { repeated, left, right, wide, wider, narrow, thrice, once, two, none };
    for (size_t i = 0; i < sizeof all / sizeof *all; i++) mt_drop(all[i]);
    return done(m);
}
