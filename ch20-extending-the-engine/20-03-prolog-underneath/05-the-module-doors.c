/* Purpose: six doors that load Prolog, and what separates them. consult_file
 *   loads every clause of a file and use_module_file only what a module
 *   exports, each registered afterwards by import_prolog_functions; the
 *   let-form use_module_global puts the path in its answer position;
 *   import_prolog_functions_from_module and lib_zar's two _pred spellings
 *   load and register in one call; use-module! loads a library of SWI's own;
 *   static-import! compiles a file of data rows into the space. C's model of
 *   each fixture predicate is the factor it multiplies by, so every function
 *   answers what C multiplies, and a hidden predicate a module does not
 *   export is refused with the ball C builds. The static rows are C's own
 *   table: match finds exactly the cities C lists for France, sorted by
 *   mt_order, and the population it lists. Whether static-import! left its
 *   two images beside the source is asked of the file system through the
 *   engine and through access(), and C removes them with remove(), after
 *   which neither says they are there.
 * Guarantees: all twenty-one claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* A door the program needs before it can go on: on refusal, say why and stop. */
#define require(what, ok) \
    ((ok) ? (void)0 : (fprintf(stderr, "%s: %s\n", (what), mt_errmsg()), exit(EXIT_FAILURE)))

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

/* Whether an atom is want up to renaming variables; takes both, and shows
   them when they differ. */
static inline bool atom_is(mt_atom *got, mt_atom *want)
{
    bool holds = got && want && mt_alpha_eq(got, want);
    if (!holds) fprintf(stderr, "  got %s\n  want %s\n", got ? mt_show(got) : "nothing",
                        want ? mt_show(want) : "nothing");
    mt_drop(got);
    mt_drop(want);
    return holds;
}

#define FIXTURES "./examples/ch20-extending-the-engine/20-03-prolog-underneath/_fixtures/"

/* What each fixture predicate multiplies its argument by. */
static const struct { const char *function; int64_t factor; } factors[] = {
    { "rung_double", 2 }, { "rung_module_triple", 3 }, { "rung_pred_quad", 4 },
    { "rung_pred_five", 5 }, { "rung_global_six", 6 },
};

static int64_t answers(const char *function, int64_t x)
{
    for (size_t i = 0; i < sizeof factors / sizeof *factors; i++)
        if (strcmp(factors[i].function, function) == 0) return factors[i].factor * x;
    require("a fixture predicate C models", false);
    return 0;
}

static void answers_as_modelled(metta *m, const char *function, int64_t x)
{
    assert(mt_one_int(mt_eval(m, E(function, x))) == answers(function, x) && function);
}

/* The static rows the fixture holds, as C lists them. */
static const struct { const char *city, *country; } cities[] = {
    { "paris", "france" }, { "lyon", "france" }, { "berlin", "germany" },
};
static const struct { const char *city; int64_t people; } populations[] = { { "paris", 2100000 } };

static const char *const images[] = { FIXTURES "static_rows.tokens-v1.pl", FIXTURES "static_rows.tokens-v1.qlf" };

/* file-exists against access(), once the file system says whether PATH is
   there as expected. */
static void exists_as_the_file_system_says(metta *m, const char *path, bool expected)
{
    bool there = access(path, F_OK) == 0;
    require(expected ? "the image is there" : "the image is gone", there == expected);
    assert(answers_are(mt_eval(m, E("file-exists", T(path))), E(B(there))) && "file-exists agrees with access()");
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    static const char *const libraries[] = { "lib_import", "lib_zar", "lib_file" };
    for (size_t i = 0; i < sizeof libraries / sizeof *libraries; i++)
        require(libraries[i], mt_one_truth(mt_eval(m, E("import!", "&self", E("library", libraries[i])))));

    assert(answers_are(mt_eval(m, E("consult_file", T(FIXTURES "rung_functions.pl"))), E(B(true))) && "consult_file loads every clause");
    assert(answers_are(mt_eval(m, E("import_prolog_functions", E("noeval", E("rung_double", "rung_greeting")))), E(B(true)))
           && "registration is its own step");
    answers_as_modelled(m, "rung_double", 21);

    assert(answers_are(mt_eval(m, E("use_module_file", T(FIXTURES "rung_module.pl"))), E(B(true)))
           && "use_module_file loads a module's exports");
    assert(answers_are(mt_eval(m, E("import_prolog_functions", E("noeval", E("rung_module_triple")))), E(B(true)))
           && "an export registers");
    answers_as_modelled(m, "rung_module_triple", 14);
    mt_atom *hidden = mt_first(mt_eval(m, E("catch", E("import_prolog_functions", E("noeval", E("rung_module_hidden"))))));
    require("the hidden one is refused", hidden && mt_kind_of(hidden) == MT_EXPR && mt_len(hidden) == 3);
    assert(atom_is(mt_keep(mt_at(hidden, 1)), E("existence_error", "procedure", "rung_module_hidden"))
           && "a name the module hides has no predicate");
    mt_drop(hidden);

    assert(answers_are(mt_eval(m, E("let", T(FIXTURES "rung_global.pl"), E("use_module_global"), "loaded")), E("loaded"))
           && "use_module_global takes the path in its answer position");
    assert(answers_are(mt_eval(m, E("import_prolog_functions_from_module", T(FIXTURES "rung_global.pl"), E("rung_global_six"))), E(B(true)))
           && "from_module loads and registers in one call");
    answers_as_modelled(m, "rung_global_six", 7);
    assert(answers_are(mt_eval(m, E("import_prolog_functions_from_file_pred", T(FIXTURES "rung_pred_file.pl"), E("rung_pred_quad"))), E(B(true)))
           && "lib_zar's file spelling");
    answers_as_modelled(m, "rung_pred_quad", 10);
    assert(answers_are(mt_eval(m, E("import_prolog_functions_from_module_pred", T(FIXTURES "rung_pred_module.pl"),
                                    E("rung_pred_five"))), E(B(true)))
           && "and its module spelling");
    answers_as_modelled(m, "rung_pred_five", 10);
    assert(answers_are(mt_eval(m, E("use-module!", "lists")), E(B(true))) && "use-module! loads a library of SWI's own");

    assert(answers_are(mt_eval(m, E("static-import!", mt_spaceref("&self"), T(FIXTURES "static_rows"))), E(B(true)))
           && "static-import! compiles the rows into the space");
    mt_list french = mt_all(mt_match(m, E("city", V("c"), "france")));
    for (size_t i = 0; i < french.len; i++) {
        mt_atom *city = mt_keep(mt_at(french.items[i], 1));
        mt_drop(french.items[i]);
        french.items[i] = city;
    }
    qsort(french.items, french.len, sizeof *french.items, mt_order);
    mt_atom *want[sizeof cities / sizeof *cities];
    size_t n = 0;
    for (size_t i = 0; i < sizeof cities / sizeof *cities; i++)
        if (strcmp(cities[i].country, "france") == 0) want[n++] = S(cities[i].city);
    qsort(want, n, sizeof *want, mt_order);
    assert(list_is(french, mt_exprv(n, want)) && "match finds the rows C lists");
    assert(answers_are(mt_match(m, E("population", populations[0].city, V("n"))), E(E("population", populations[0].city, populations[0].people)))
           && "and the population it lists");

    for (size_t i = 0; i < 2; i++) exists_as_the_file_system_says(m, images[i], true);
    for (size_t i = 0; i < 2; i++) require("remove the image", remove(images[i]) == 0);
    exists_as_the_file_system_says(m, images[0], false);
    mt_close(m);
    return 0;
}
