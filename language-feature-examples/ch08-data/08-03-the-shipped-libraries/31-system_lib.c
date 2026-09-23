/* Purpose: lib_system, held against POSIX calls in the same process, which is
 *   the point: the engine and C share one environment and one working
 *   directory, so what the engine writes C reads with getenv, and what C
 *   reads with getcwd is what the engine answers. The whole environment is
 *   environ, a relation because every entry is NAME=VALUE. The platform
 *   answers come from what C knows the same way: the family from C's own
 *   __unix__, the cores from sysconf(_SC_NPROCESSORS_ONLN), which is SWI's own
 *   call for its cpu_count flag (swipl-devel src/os/pl-os.c, CpuCount), and
 *   the dialect from what cmetta embeds. The version's text and numbers are
 *   read apart and compared by C. A directory C cannot stat is one the engine
 *   refuses to move into, and C never moves itself.
 * Guarantees: all twenty-nine claims of the original hold [tested: make
 *   twins; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"
#include <inttypes.h>
#include <sys/stat.h>
#include <unistd.h>

extern char **environ;

/* The library's platform keys, a vocabulary C holds as data [source:
   lib/lib_system/lib_system.pl, platform_key/1;
   commit=33c2d50c84b24c1a2c906225600e2a4ffdb662f1]. */
static const char *const keys[] = { "architecture", "family", "version", "version-numbers", "dialect",
                                    "pid",          "cores",  "bounded-integers", "executable", "prolog-home" };

static bool known_key(const char *key)
{
    for (size_t i = 0; i < sizeof keys / sizeof *keys; i++)
        if (strcmp(keys[i], key) == 0) return true;
    return false;
}

static const char *family(void)
{
#if defined(__unix__)
    return "unix";
#else
    return "other";
#endif
}

/* A variable's value as the engine answers it: none when unset. */
static mt_atom *value_of_env(const char *name)
{
    const char *v = getenv(name);
    return v ? E(T(v)) : mt_unit();
}

static bool relation_of_environ(void)
{
    for (char **e = environ; *e; e++)
        if (!strchr(*e, '=')) return false;
    return true;
}

static bool is_dir(const char *path)
{
    struct stat s;
    return stat(path, &s) == 0 && S_ISDIR(s.st_mode);
}

/* A write to the environment: the engine's cursor runs it only when read, so
   C reads the answer first and only then looks with getenv. The claim is
   that the write answers True exactly when C sees what it wrote, or, for an
   unset, sees nothing. */
static void check_write(const char *claim, metta *m, mt_atom *goal, const char *name, const char *wrote)
{
    mt_atom *answer = mt_one(mt_eval(m, goal));
    const char *now = getenv(name);
    check_atom(claim, answer, B(wrote ? now && strcmp(now, wrote) == 0 : now == NULL));
}

static mt_atom *verdict(bool holds) { return S(holds ? "fine" : "refused"); }
static mt_atom *guarded(mt_atom *goal) { return E("if-error", E("catch", goal), "refused", "fine"); }

static mt_atom *value_of(metta *m, mt_atom *goal)
{
    mt_atom *v = mt_one(mt_eval(m, goal));
    require("a value", v != NULL);
    return v;
}

int main(void)
{
    metta *m = open_engine();
    static const char *const libraries[] = { "lib_system", "lib_pairs", "lib_string" };
    for (size_t i = 0; i < 3; i++)
        require("import", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", libraries[i])))));
    const char *absent = "NO_SUCH_VARIABLE_HERE", *name = "METTA_SYSTEM_EXAMPLE";
    require("the variables start unset", !getenv(absent) && !getenv(name));

    /* Unset has no answer; a write is what getenv reads next. */
    check_answers("unset", mt_eval(m, E("collapse", E("env-get", T(absent)))), value_of_env(absent));
    check_answers("a presence test", mt_eval(m, E("if", E("==", mt_unit(), E("collapse", E("env-get", T(absent)))), "unset", "set")),
                  S(getenv(absent) ? "set" : "unset"));
    check_write("env-set!", m, E("env-set!", T(name), T("on")), name, "on");
    check_answers("which getenv reads", mt_eval(m, E("env-get", T(name))), T(getenv(name)));
    check_write("an empty value", m, E("env-set!", T(name), T("")), name, "");
    check_answers("is set", mt_eval(m, E("env-get", T(name))), T(getenv(name)));
    check_answers("and present", mt_eval(m, E("if", E("==", mt_unit(), E("collapse", E("env-get", T(name)))), "unset", "set")),
                  S(getenv(name) ? "set" : "unset"));
    check_write("env-unset!", m, E("env-unset!", T(name)), name, NULL);
    check_answers("then it is gone", mt_eval(m, E("collapse", E("env-get", T(name)))), value_of_env(name));
    check_write("removing it again is silent", m, E("env-unset!", T(name)), name, NULL);

    /* The whole environment, as environ holds it. */
    check_write("set to list", m, E("env-set!", T(name), T("listed")), name, "listed");
    check_answers("the environment is a relation", mt_eval(m, E("pairs-is", E("env-all"))), B(relation_of_environ()));
    check_answers("which holds the value", mt_eval(m, E("collapse", E("pairs-lookup", E("env-all"), T(name)))), value_of_env(name));
    check_answers("and nothing unset", mt_eval(m, E("collapse", E("pairs-lookup", E("env-all"), T(absent)))), value_of_env(absent));
    check_write("cleaned up", m, E("env-unset!", T(name)), name, NULL);

    /* The platform. */
    check_answers("the family", mt_eval(m, E("platform-info", "family")), T(family()));
    check_answers("the dialect cmetta embeds", mt_eval(m, E("platform-info", "dialect")), T("swi"));
    check_answers("the cores are the processors online", mt_eval(m, E("platform-info", "cores")), (int64_t)sysconf(_SC_NPROCESSORS_ONLN));
    mt_atom *version = value_of(m, E("platform-info", "version")), *numbers = value_of(m, E("platform-info", "version-numbers"));
    char copy[64], *save;
    size_t parts = 0;
    snprintf(copy, sizeof copy, "%s", mt_name(version));
    for (char *p = strtok_r(copy, ".", &save); p; p = strtok_r(NULL, ".", &save)) parts++;
    check_answers("a version number per part of its text", mt_eval(m, E("size-atom", E("platform-info", "version-numbers"))), (int64_t)parts);
    check_answers("every key", mt_eval(m, E("size-atom", E("platform-keys"))), (int64_t)(sizeof keys / sizeof *keys));
    check_answers("an unknown key is refused", mt_eval(m, guarded(E("platform-info", "nosuch"))), verdict(known_key("nosuch")));
    char major[24];
    snprintf(major, sizeof major, "%" PRId64, mt_int(mt_at(numbers, 0)));
    check_answers("the text starts with the major number",
                  mt_eval(m, E("string-starts-with", E("platform-info", "version"),
                               E("number-to-string", E("car-atom", E("platform-info", "version-numbers"))))),
                  B(strncmp(mt_name(version), major, strlen(major)) == 0));

    /* The working directory is getcwd's. */
    char cwd[4096];
    require("getcwd", getcwd(cwd, sizeof cwd) != NULL);
    size_t len = strlen(cwd);
    check_answers("absolute", mt_eval(m, E("string-starts-with", E("working-directory"), T("/"))), B(cwd[0] == '/'));
    check_answers("without a trailing separator", mt_eval(m, E("string-ends-with", E("working-directory"), T("/"))), B(len > 1 && cwd[len - 1] == '/'));
    const char *nowhere = "/no/such/directory/here";
    check_answers("no moving into what is not there", mt_eval(m, guarded(E("change-directory!", T(nowhere)))), verdict(is_dir(nowhere)));
    char after[4096];
    require("getcwd", getcwd(after, sizeof after) != NULL);
    check_answers("and the directory stays", mt_eval(m, E("string-starts-with", E("working-directory"), T("/"))), B(strcmp(after, cwd) == 0 && after[0] == '/'));

    /* Refusals. */
    mt_atom *seven = mt_num(7), *nosuch = E("nosuch");
    check_answers("a number is no name", mt_eval(m, guarded(E("env-get", mt_keep(seven)))), verdict(mt_kind_of(seven) == MT_TEXT));
    check_answers("an expression is no value", mt_eval(m, guarded(E("env-set!", T("A"), mt_keep(nosuch)))), verdict(mt_kind_of(nosuch) == MT_TEXT));
    check_answers("a number is no directory", mt_eval(m, guarded(E("change-directory!", mt_keep(seven)))), verdict(mt_kind_of(seven) == MT_TEXT));

    mt_atom *held[] = { version, numbers, seven, nosuch };
    for (size_t i = 0; i < sizeof held / sizeof *held; i++) mt_drop(held[i]);
    return done(m);
}
