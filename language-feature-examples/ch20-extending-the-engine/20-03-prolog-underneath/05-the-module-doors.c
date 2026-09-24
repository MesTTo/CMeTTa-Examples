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
 * Guarantees: all twenty-one claims of the original hold [tested: make
 *   twins; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include <unistd.h>

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
    check_int(function, mt_one_int(mt_eval(m, E(function, x))), answers(function, x));
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
    check_answers("file-exists agrees with access()", mt_eval(m, E("file-exists", T(path))), B(there));
}

int main(void)
{
    metta *m = open_engine();
    static const char *const libraries[] = { "lib_import", "lib_zar", "lib_file" };
    for (size_t i = 0; i < sizeof libraries / sizeof *libraries; i++)
        require(libraries[i], mt_one_truth(mt_eval(m, E("import!", "&self", E("library", libraries[i])))));

    check_answers("consult_file loads every clause", mt_eval(m, E("consult_file", T(FIXTURES "rung_functions.pl"))), B(true));
    check_answers("registration is its own step",
                  mt_eval(m, E("import_prolog_functions", E("noeval", E("rung_double", "rung_greeting")))), B(true));
    answers_as_modelled(m, "rung_double", 21);

    check_answers("use_module_file loads a module's exports", mt_eval(m, E("use_module_file", T(FIXTURES "rung_module.pl"))),
                  B(true));
    check_answers("an export registers", mt_eval(m, E("import_prolog_functions", E("noeval", E("rung_module_triple")))),
                  B(true));
    answers_as_modelled(m, "rung_module_triple", 14);
    mt_atom *hidden = mt_first(mt_eval(m, E("catch", E("import_prolog_functions", E("noeval", E("rung_module_hidden"))))));
    require("the hidden one is refused", hidden && mt_kind_of(hidden) == MT_EXPR && mt_len(hidden) == 3);
    check_atom("a name the module hides has no predicate", mt_keep(mt_at(hidden, 1)),
               E("existence_error", "procedure", "rung_module_hidden"));
    mt_drop(hidden);

    check_answers("use_module_global takes the path in its answer position",
                  mt_eval(m, E("let", T(FIXTURES "rung_global.pl"), E("use_module_global"), "loaded")), "loaded");
    check_answers("from_module loads and registers in one call",
                  mt_eval(m, E("import_prolog_functions_from_module", T(FIXTURES "rung_global.pl"), E("rung_global_six"))),
                  B(true));
    answers_as_modelled(m, "rung_global_six", 7);
    check_answers("lib_zar's file spelling",
                  mt_eval(m, E("import_prolog_functions_from_file_pred", T(FIXTURES "rung_pred_file.pl"), E("rung_pred_quad"))),
                  B(true));
    answers_as_modelled(m, "rung_pred_quad", 10);
    check_answers("and its module spelling",
                  mt_eval(m, E("import_prolog_functions_from_module_pred", T(FIXTURES "rung_pred_module.pl"),
                               E("rung_pred_five"))),
                  B(true));
    answers_as_modelled(m, "rung_pred_five", 10);
    check_answers("use-module! loads a library of SWI's own", mt_eval(m, E("use-module!", "lists")), B(true));

    check_answers("static-import! compiles the rows into the space",
                  mt_eval(m, E("static-import!", mt_spaceref("&self"), T(FIXTURES "static_rows"))), B(true));
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
    check_list_("match finds the rows C lists", french, n, want);
    check_answers("and the population it lists", mt_match(m, E("population", populations[0].city, V("n"))),
                  E("population", populations[0].city, populations[0].people));

    for (size_t i = 0; i < 2; i++) exists_as_the_file_system_says(m, images[i], true);
    for (size_t i = 0; i < 2; i++) require("remove the image", remove(images[i]) == 0);
    exists_as_the_file_system_says(m, images[0], false);
    return done(m);
}
