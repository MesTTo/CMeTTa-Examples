/* Purpose: lib_conformance proving a Prolog space provider, the fixture the
 *   original loads, through the seam's own checks. Two lines of the report
 *   count what the provider holds, and C counts it too, enumerating the
 *   provider's atoms through its own space handle. A match through the seam
 *   is held against C matching for itself: mt_unify of the pattern against
 *   every atom the provider lists, the bindings sorted with mt_order.
 * Guarantees: both claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

enum { MOST = 16 };

int main(void)
{
    metta *m = open_engine();
    require("import lib_conformance",
            mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_conformance")))));
    /* The path the original writes, relative to the engine tree the lane
       runs both from. */
    mt_list loaded = mt_all(mt_eval(m, E("import_prolog_functions_from_file",
        T("./examples/ch08-data/08-03-the-shipped-libraries/_fixtures/demo_provider.pl"), mt_unit())));
    require("the provider loads", mt_ok());
    mt_list_free(loaded);

    mt_space *demo = mt_space_open(m, "&demo_provider");
    require("a handle on the provider's space", demo != NULL);
    mt_list atoms = mt_all(mt_space_atoms(demo));
    require("the provider lists its atoms", mt_ok() && atoms.len > 0 && atoms.len < MOST);

    char families[128], pushdown[64];
    snprintf(families, sizeof families,
             "match: over-approximation holds over %zu atoms and their pattern families", atoms.len);
    snprintf(pushdown, sizeof pushdown, "pushdown: 0 of %zu patterns claimed exact, and are", atoms.len);
    check_answers("the checks that ran, in order", mt_eval(m, E("check-space-provider", mt_spaceref("&demo_provider"))),
                  E(T("match: declared, seam:foreign_match/3 has clauses"),
                    T("enumerate: declared, seam:foreign_atoms/2 has clauses"),
                    T(families),
                    T("source: repeated, two enumerations agree"),
                    T("round trip: not asked, the provider does not declare add, remove and enumerate together"),
                    T(pushdown),
                    T("plan: not declared, so a conjunction takes the engine's split")));

    /* C's own match over what the provider lists. */
    mt_atom *pattern = E("edge", "a", V("y")), *reached[MOST];
    size_t n = 0;
    for (size_t i = 0; i < atoms.len; i++) {
        mt_bindings *b = mt_unify(pattern, atoms.items[i]);
        if (b) reached[n++] = mt_keep(mt_binding(b, "y"));
        mt_bindings_free(b);
    }
    qsort(reached, n, sizeof *reached, mt_order);
    check_answers("and the provider answers through the seam",
                  mt_eval(m, E("sort-atom", E("collapse", E("match", mt_spaceref("&demo_provider"),
                                                            mt_keep(pattern), V("y"))))),
                  mt_exprv(n, reached));
    mt_drop(pattern);
    mt_list_free(atoms);
    mt_space_close(demo);
    return done(m);
}
