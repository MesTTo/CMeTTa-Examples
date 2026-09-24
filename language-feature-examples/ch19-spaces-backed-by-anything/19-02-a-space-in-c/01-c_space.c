/* Purpose: a space whose atoms live in C. The original puts four multifile
 *   clauses on the foreign-space seam in cstore.pl and keeps the atoms in
 *   cstore.c as text; in C the store is c_store.h's C array behind cmetta's
 *   own provider door, mt_provider_open, holding each atom it is given under
 *   a pthread mutex and answering each match with a snapshot of the array, so
 *   the engine keeps unification for itself and filters what C enumerates.
 *   remove-atom drains every unifying atom through the provider's
 *   one-occurrence remove. The seam's proof harness is held to the report C
 *   expects from the callbacks it supplied and the atoms it holds. The
 *   original's concurrent writers are four pthreads, each attached to the
 *   engine, and the mutex keeps every row whole.
 * Guarantees: all six guarded claims of the original hold [tested: make
 *   twins; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"
#include "c_store.h"

static c_store cstore = C_STORE_INIT;

/* The harness's report for this provider: a declared line per capability C
   supplied, in the order the harness reads them, then its behavioural lines
   over the atoms the store holds. */
static mt_atom *expected_report(size_t atoms)
{
    static const struct { const char *capability, *clause; } declared[] = {
        { "match", "seam:foreign_match/3" }, { "enumerate", "seam:foreign_atoms/2" }, { "add", "seam:foreign_add/2" },
        { "remove", "seam:foreign_remove/3" }, { "clear", "seam:foreign_clear/1" },
    };
    enum { DECLARED = sizeof declared / sizeof *declared, LINES = DECLARED + 5 };
    char text[LINES][128];
    mt_atom *lines[LINES];
    for (size_t i = 0; i < DECLARED; i++)
        snprintf(text[i], sizeof text[i], "%s: declared, %s has clauses", declared[i].capability, declared[i].clause);
    snprintf(text[DECLARED], sizeof text[0], "match: over-approximation holds over %zu atoms and their pattern families", atoms);
    snprintf(text[DECLARED + 1], sizeof text[0], "source: repeated, two enumerations agree");
    snprintf(text[DECLARED + 2], sizeof text[0], "round trip: add then enumerate answers the atom, and remove takes it back");
    snprintf(text[DECLARED + 3], sizeof text[0], "pushdown: 0 of %zu patterns claimed exact, and are", atoms);
    snprintf(text[DECLARED + 4], sizeof text[0], "plan: not declared, so a conjunction takes the engine's split");
    for (size_t i = 0; i < LINES; i++) lines[i] = mt_text(text[i]);
    return mt_exprv(LINES, lines);
}

/* A writer thread: one (row n) through the provider, attached to the engine
   for the one write. */
typedef struct writer {
    mt_space *space;
    int64_t row;
    bool wrote;
} writer;

static void *write_row(void *opaque)
{
    writer *w = opaque;
    if (!mt_thread_attach()) return NULL;
    w->wrote = mt_add(w->space, E("row", w->row));
    mt_thread_detach();
    return NULL;
}

int main(void)
{
    metta *m = open_engine();
    /* The original's imports: lib_conformance holds the harness asked below,
       and its loader needs lib_import and its guard lib_file, which C does
       not, but &self holds what they define on both sides. */
    static const char *const libraries[] = { "lib_import", "lib_file", "lib_conformance" };
    for (size_t i = 0; i < sizeof libraries / sizeof *libraries; i++)
        require(libraries[i], mt_one_truth(mt_eval(m, E("import!", "&self", E("library", libraries[i])))));
    require("the store backs &cstore",
            mt_provider_open(m, "&cstore", c_store_provider(&cstore, false)));
    mt_space *space = mt_space_open(m, "&cstore");
    require("a handle on &cstore", space != NULL);

    static const char *const edges[][2] = { { "a", "b" }, { "a", "c" }, { "b", "c" } };
    for (size_t i = 0; i < 3; i++) require("write into C", mt_add(space, E("edge", edges[i][0], edges[i][1])));
    check_answers("the engine filters what C enumerates", mt_eval(m, E("match", mt_spaceref("&cstore"), E("edge", "a", V("x")), V("x"))),
                  "b", "c");

    require("drain (edge a $any)", mt_one_truth(mt_eval(m, E("remove-atom", mt_spaceref("&cstore"), E("edge", "a", V("any"))))));
    check_answers("every unifying atom went", mt_match(space, E("edge", V("x"), V("y"))), E("edge", "b", "c"));

    for (int i = 0; i < 3; i++) require("a copy", mt_add(space, E("dup", 1)));
    require("one removal", mt_one_truth(mt_eval(m, E("remove-atom", mt_spaceref("&cstore"), E("dup", 1)))));
    check_none("clears all three copies", mt_match(space, E("dup", V("n"))));
    check_answers("and draining again is idempotent", mt_eval(m, E("remove-atom", mt_spaceref("&cstore"), E("dup", 1))), B(true));

    check_answers("the seam's harness proves the provider", mt_eval(m, E("check-space-provider", mt_spaceref("&cstore"))),
                  expected_report(c_store_held(&cstore)));

    enum { WRITERS = 4 };
    writer writers[WRITERS];
    pthread_t threads[WRITERS];
    for (int i = 0; i < WRITERS; i++) {
        writers[i] = (writer){ .space = space, .row = i + 1 };
        require("start a writer", pthread_create(&threads[i], NULL, write_row, &writers[i]) == 0);
    }
    bool all_wrote = true;
    for (int i = 0; i < WRITERS; i++) {
        require("join a writer", pthread_join(threads[i], NULL) == 0);
        all_wrote = all_wrote && writers[i].wrote;
    }
    require("every writer wrote", all_wrote);
    int64_t rows = 0;
    mt_each (row, mt_match(space, E("row", V("n")))) rows++;
    check_int("concurrent writers keep every row whole", rows, WRITERS);

    mt_space_close(space);
    require("withdraw the provider", mt_provider_close(m, "&cstore"));
    return done(m);
}
