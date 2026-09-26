/* Purpose: the fixtures section 20-04's originals import, named from the
 *   engine tree where the twins run, since a C program has no importing file
 *   for a relative import to resolve against; an import of one of them into
 *   a space, as the originals' import! forms are; and a pragma, the setting
 *   their reference rows read.
 * Assumes: the includer defines MT_SHORTHAND before its first include.
 */
#ifndef CH20_MODULES_H
#define CH20_MODULES_H
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>

/* A door the program needs before it can go on: on refusal, say why and stop. */
#define require(what, ok) \
    ((ok) ? (void)0 : (fprintf(stderr, "%s: %s\n", (what), mt_errmsg()), exit(EXIT_FAILURE)))

#define MODULES_FIXTURES "examples/ch20-extending-the-engine/20-04-modules-and-the-catalog/_fixtures/"

/* (import! space <fixture>), which must succeed. */
static inline void import_fixture(metta *m, const char *space, const char *fixture)
{
    char path[512];
    snprintf(path, sizeof path, "%s%s", MODULES_FIXTURES, fixture);
    require(path, mt_one_truth(mt_eval(m, E("import!", mt_spaceref(space), S(path)))));
}

/* (pragma! setting value), which answers the unit and must succeed; every
   answer is taken, since a cursor freed unstepped runs nothing. TAKES
   value. */
static inline void pragma(metta *m, const char *setting, mt_atom *value)
{
    mt_list_free(mt_all(mt_eval(m, E("pragma!", setting, value))));
    require(setting, mt_ok());
}

static inline void import_library(metta *m, const char *library)
{
    require(library, mt_one_truth(mt_eval(m, E("import!", "&self", E("library", library)))));
}
#endif
