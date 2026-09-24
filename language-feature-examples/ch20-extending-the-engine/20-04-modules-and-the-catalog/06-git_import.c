/* Purpose: a library imported from a git repository. The fixture's Prolog
 *   builds the repository locally and answers its URL, registered as a MeTTa
 *   function through mt_register_prolog, the C seat's door onto the engine's
 *   registration sequence; git-import! clones it under ./repos, the clone is
 *   an ordinary library from then on, and its function answers what C's
 *   model of the fixture's one equation computes, three times its argument.
 * Guarantees: the original's claim holds [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include "modules.h"

enum { FIXTURE_FACTOR = 3 };

int main(void)
{
    metta *m = open_engine();
    import_library(m, "lib_import");
    mt_atom *registered = mt_register_prolog(m, (mt_prolog){ MT_PROLOG_FILE, "./" MODULES_FIXTURES "git_fixture.pl" },
                                             E("git_fixture_url"));
    mt_atom *want = E("git_fixture_url");
    require("register the fixture's URL function", registered && mt_eq(registered, want));
    mt_drop(registered), mt_drop(want);
    require("clone it", mt_one_truth(mt_eval(m, E("git-import!", E("git_fixture_url", T("./repos"))))));
    require("import the clone", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "metta_fixture_lib", "fixture")))));
    const int64_t x = 14;
    check_int("the cloned library's function", mt_one_int(mt_eval(m, E("fixture-answer", x))), FIXTURE_FACTOR * x);
    return done(m);
}
