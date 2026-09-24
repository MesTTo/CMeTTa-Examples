/* Purpose: a space whose atoms live in MORK's Rust trie, reached through
 *   lib_mm2's five operators over &mork, each built in C as the term it is,
 *   the full-width ＋ and － being their UTF-8 symbols. C keeps the relation
 *   it expects &mork to hold in its own table: ＋ and ＋* add rows, － and
 *   the transforms take them away, and ~> with mm2-exec derives path from
 *   edge and then replaces path with route, which C derives from the same
 *   table. Every query answer is sorted by mt_order and held to C's rows.
 *   require-extension! refuses an unknown seat by name, as a structure C
 *   compares, and answers the unit for a loaded one. The MORK half is asked
 *   for only when its library is built, as the original asks, and C asks
 *   the file system.
 * Guarantees: both unguarded claims of the original hold, and its seven
 *   guarded ones whenever MORK is built [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include <unistd.h>

static const char *const mork_library = "./extensions/mork/mork_ffi/target/release/libmork_ffi.so";

/* The rows of one binary relation C expects &mork to hold. */
typedef struct row {
    const char *from, *to;
} row;

static const row edges[] = { { "a", "b" }, { "b", "c" }, { "c", "d" } };
#define EDGES (sizeof edges / sizeof *edges)

static metta *engine;

/* (? (head $x $y) ($x $y)) sorted, against C's rows sorted the same way. */
static void holds(const char *claim, const char *head, const row *rows, size_t n)
{
    mt_list got = mt_all(mt_eval(engine, E("?", E(head, V("x"), V("y")), E(V("x"), V("y")))));
    qsort(got.items, got.len, sizeof *got.items, mt_order);
    mt_atom *want[EDGES];
    for (size_t i = 0; i < n; i++) want[i] = E(rows[i].from, rows[i].to);
    qsort(want, n, sizeof *want, mt_order);
    check_list_(claim, got, n, want);
}

static void run(const char *what, mt_atom *goal)
{
    mt_list answers = mt_all(mt_eval(engine, goal));
    require(what, mt_ok());
    mt_list_free(answers);
}

int main(void)
{
    metta *m = engine = open_engine();
    require("import lib_file", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_file")))));
    check_answers("an unknown seat is refused by name", mt_eval(m, E("catch", E("require-extension!", "nosuchseat"))),
                  E("Error", E("metta_extension_required", "nosuchseat", "unknown"), "none"));
    check_answers("a loaded seat answers the unit", mt_eval(m, E("require-extension!", "python")), mt_exprv(0, NULL));

    if (access(mork_library, F_OK) != 0) {
        printf("SKIPPED mm2-operators: libmork_ffi.so is not built, see extensions/mork/build.sh\n");
        return done(m);
    }
    require("import lib_mm2", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_mm2")))));

    run("＋ one edge", E("＋", E("edge", edges[0].from, edges[0].to)));
    holds("＋ is add-atom on &mork", "edge", edges, 1);
    run("－ it", E("－", E("edge", edges[0].from, edges[0].to)));
    holds("and － is remove-atom", "edge", edges, 0);

    mt_atom *batch[EDGES];
    for (size_t i = 0; i < EDGES; i++) batch[i] = E("edge", edges[i].from, edges[i].to);
    run("＋* the batch", E("＋*", mt_exprv(EDGES, batch)));
    holds("＋* adds a whole batch in one crossing", "edge", edges, EDGES);

    run("mork-add-atoms", E("mork-add-atoms", mt_spaceref("&mork"), E(E("tag", 1), E("tag", 2))));
    run("mork-flush", E("mork-flush", mt_spaceref("&mork")));
    mt_list tags = mt_all(mt_eval(m, E("?", E("tag", V("n")), V("n"))));
    qsort(tags.items, tags.len, sizeof *tags.items, mt_order);
    check_list("a flush makes queued additions visible", tags, 1, 2);

    run("~> derives path", E("~>", E(",", E("edge", V("x"), V("y"))), E("O", E("+", E("path", V("x"), V("y"))))));
    holds("the transform is MM2's own calculus", "path", edges, EDGES);
    run("~> replaces path with route",
        E("~>", E(",", E("path", V("x"), V("y"))), E("O", E("-", E("path", V("x"), V("y"))), E("+", E("route", V("x"), V("y"))))));
    run("mm2-exec a step", E("mm2-exec", mt_spaceref("&mork"), 1));
    holds("a rule that removes and adds replaces", "route", edges, EDGES);
    holds("so no path is left", "path", edges, 0);

    static const char *const relations[] = { "edge", "route" };
    for (size_t r = 0; r < 2; r++)
        run("leave &mork as found",
            E("collapse", E("let", E(V("x"), V("y")), E("?", E(relations[r], V("x"), V("y")), E(V("x"), V("y"))),
                            E("－", E(relations[r], V("x"), V("y"))))));
    run("and its tags", E("collapse", E("let", V("n"), E("?", E("tag", V("n")), V("n")), E("－", E("tag", V("n"))))));
    return done(m);
}
