/* Purpose: lib_conformance proving a Prolog space provider, the fixture the
 *   original loads, through the seam's own checks. Two lines of the report
 *   count what the provider holds, and C counts it too, enumerating the
 *   provider's atoms through its own space handle. A match through the seam
 *   is held against C matching for itself: mt_unify of the pattern against
 *   every atom the provider lists, the bindings sorted with mt_order. The
 *   provider and its report live in conformance_report.h, which
 *   16-the_prolog_rung shares.
 * Guarantees: both claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include "_fixtures/conformance_report.h"

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

enum { MOST = 16 };

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_conformance",
            mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_conformance")))));
    mt_list atoms = demo_provider(m);
    assert(answers_are(mt_eval(m, E("check-space-provider", mt_spaceref("&demo_provider"))), E(demo_report(atoms.len)))
           && "the checks that ran, in order");

    /* C's own match over what the provider lists. */
    mt_atom *pattern = E("edge", "a", V("y")), *reached[MOST];
    size_t n = 0;
    for (size_t i = 0; i < atoms.len; i++) {
        mt_bindings *b = mt_unify(pattern, atoms.items[i]);
        if (b) reached[n++] = mt_keep(mt_binding(b, "y"));
        mt_bindings_free(b);
    }
    qsort(reached, n, sizeof *reached, mt_order);
    assert(answers_are(mt_eval(m, E("sort-atom", E("collapse", E("match", mt_spaceref("&demo_provider"),
                                                                 mt_keep(pattern), V("y"))))), E(mt_exprv(n, reached)))
           && "and the provider answers through the seam");
    mt_drop(pattern);
    mt_list_free(atoms);
    mt_close(m);
    return 0;
}
