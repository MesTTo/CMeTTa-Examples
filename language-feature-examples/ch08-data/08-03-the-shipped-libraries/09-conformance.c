/* Purpose: lib_conformance proving a Prolog space provider, the fixture the
 *   original loads, through the seam's own checks. Two lines of the report
 *   count what the provider holds, and C counts it too, enumerating the
 *   provider's atoms through its own space handle. A match through the seam
 *   is held against C matching for itself: mt_unify of the pattern against
 *   every atom the provider lists, the bindings sorted with mt_order. The
 *   provider and its report live in conformance_report.h, which
 *   16-the_prolog_rung shares.
 * Guarantees: both claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"
#include "conformance_report.h"

enum { MOST = 16 };

int main(void)
{
    metta *m = open_engine();
    require("import lib_conformance",
            mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_conformance")))));
    mt_list atoms = demo_provider(m);
    check_answers("the checks that ran, in order", mt_eval(m, E("check-space-provider", mt_spaceref("&demo_provider"))),
                  demo_report(atoms.len));

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
    return done(m);
}
