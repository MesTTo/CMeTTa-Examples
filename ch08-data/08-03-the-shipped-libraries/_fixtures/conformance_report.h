/* Purpose: the demo provider the conformance originals prove, and the report
 *   the seam's checks give for it, shared by 09-conformance and
 *   16-the_prolog_rung. The provider is loaded by the path the originals
 *   write, relative to the engine tree the lane runs both from, and C counts
 *   the atoms it holds through its own space handle; two lines of the report
 *   carry that count, and the rest is the fixed wording of checks the
 *   provider passes or is not asked.
 * Assumes: the includer defines MT_SHORTHAND before its first include; the working directory
 *   is the engine tree.
 */
#ifndef CONFORMANCE_REPORT_H
#define CONFORMANCE_REPORT_H
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>

/* A door the program needs before it can go on: on refusal, say why and stop. */
#define require(what, ok) \
    ((ok) ? (void)0 : (fprintf(stderr, "%s: %s\n", (what), mt_errmsg()), exit(EXIT_FAILURE)))

enum { DEMO_MOST = 16 };

/* Load the fixture provider and answer the atoms it holds, as C reads them. */
static inline mt_list demo_provider(metta *m)
{
    mt_list loaded = mt_all(mt_eval(m, E("import_prolog_functions_from_file",
        T("./examples/ch08-data/08-03-the-shipped-libraries/_fixtures/demo_provider.pl"), mt_unit())));
    require("the provider loads", mt_ok());
    mt_list_free(loaded);
    mt_space *demo = mt_space_open(m, "&demo_provider");
    require("a handle on the provider's space", demo != NULL);
    mt_list atoms = mt_all(mt_space_atoms(demo));
    require("the provider lists its atoms", mt_ok() && atoms.len > 0 && atoms.len < DEMO_MOST);
    mt_space_close(demo);
    return atoms;
}

/* The checks that run, in order, for a provider declaring match and
   enumerate, holding `atoms` atoms and claiming none of its patterns exact. */
static inline mt_atom *demo_report(size_t atoms)
{
    char families[128], pushdown[64];
    snprintf(families, sizeof families,
             "match: over-approximation holds over %zu atoms and their pattern families", atoms);
    snprintf(pushdown, sizeof pushdown, "pushdown: 0 of %zu patterns claimed exact, and are", atoms);
    return E(T("match: declared, seam:foreign_match/3 has clauses"),
             T("enumerate: declared, seam:foreign_atoms/2 has clauses"),
             T(families),
             T("source: repeated, two enumerations agree"),
             T("round trip: not asked, the provider does not declare add, remove and enumerate together"),
             T(pushdown),
             T("plan: not declared, so a conjunction takes the engine's split"));
}

#endif
