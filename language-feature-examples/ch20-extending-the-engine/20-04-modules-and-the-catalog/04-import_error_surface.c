/* Purpose: an import that cannot load is an error, whether the file is
 *   broken or missing. The original catches each into an Error value; in C
 *   the refusal is what every door reports a failure through, the error
 *   state, so C asks the import itself and reads MT_ERROR with no answer,
 *   the engine's words naming the module it could not load.
 * Guarantees: both claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"
#include "modules.h"

static void refused(metta *m, const char *claim, const char *fixture)
{
    char path[512];
    snprintf(path, sizeof path, "%s%s", MODULES_FIXTURES, fixture);
    mt_clear();
    mt_list answers = mt_all(mt_eval(m, E("import!", "&self", S(path))));
    bool failed = answers.len == 0 && mt_error() == MT_ERROR && mt_errmsg() && strstr(mt_errmsg(), fixture) != NULL;
    mt_list_free(answers);
    mt_clear();
    check(claim, failed);
}

int main(void)
{
    metta *m = open_engine();
    import_library(m, "lib_he");
    refused(m, "a broken module is an error", "imports/import_error_broken");
    refused(m, "and so is a missing one", "imports/definitely_missing_import");
    return done(m);
}
