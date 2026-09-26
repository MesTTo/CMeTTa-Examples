/* Purpose: lib_system, held against POSIX calls in the same process, which is
 *   the point: the engine and C share one environment and one working
 *   directory, so what the engine writes C reads with getenv, and what C
 *   reads with getcwd is what the engine answers. The whole environment is
 *   environ, a relation because every entry is NAME=VALUE. The platform
 *   answers come from what C knows the same way: the family from the macros
 *   SWI sets its platform flags from, the cores from
 *   sysconf(_SC_NPROCESSORS_ONLN), which is SWI's own call for its cpu_count
 *   flag (swipl-devel src/os/pl-os.c, CpuCount), and the dialect from what
 *   cmetta embeds. The version's text and numbers are read apart and compared
 *   by C. A directory C cannot stat is one the engine refuses to move into,
 *   and C never moves itself.
 * Guarantees: all twenty-nine claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define _POSIX_C_SOURCE 200809L
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <sys/stat.h>
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

extern char **environ;

/* The library's platform keys, a vocabulary C holds as data [source:
   lib/lib_system/lib_system.pl, platform_key/1;
   commit=33c2d50c84b24c1a2c906225600e2a4ffdb662f1]. */
static const char *const keys[] = { "architecture", "family", "version", "version-numbers", "dialect",
                                    "pid",          "cores",  "bounded-integers", "executable", "prolog-home" };

/* The four families a platform flag names; a host with none answers
   "unknown", which the original's claim leaves out [source
   2026-09-25T16:38:54+10:00: lib/lib_system/lib_system.pl,
   platform_value(family, _)]. */
static const char *const families[] = { "windows", "apple", "unix", "emscripten" };

/* One family as the String the library answers it. */
static mt_atom *family_text(const void *value) { return T(*(const char *const *)value); }

/* Whether a vocabulary C holds as data has a word in it. */
static bool listed(const char *const words[], size_t count, const char *word)
{
    for (size_t i = 0; i < count; i++)
        if (strcmp(words[i], word) == 0) return true;
    return false;
}

/* The family lib_system answers, from the macros SWI sets its platform flags
   from: windows under __WINDOWS__, which SWI-Prolog.h defines for _MSC_VER
   and __MINGW32__, and emscripten alone, in place of unix and apple [source
   2026-09-25T16:48:19+10:00: swipl-devel V10.1.14 src/SWI-Prolog.h:44-48,
   src/os/pl-prologflag.c:2331-2332 and 2519-2529]; lib_system reads apple
   before unix. Emscripten defines __unix__ too, so it is read first, and
   Darwin defines __APPLE__ without __unix__ [measured
   2026-09-25T16:52:41+10:00: clang 21.1.8 -dM -E for wasm32-unknown-emscripten,
   x86_64-apple-darwin and arm64-apple-macos]. */
static const char *family(void)
{
#if defined(_MSC_VER) || defined(__MINGW32__)
    return "windows";
#elif defined(__EMSCRIPTEN__)
    return "emscripten";
#elif defined(__APPLE__)
    return "apple";
#elif defined(__unix__)
    return "unix";
#else
    return "unknown";
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
    assert(atom_is(answer, B(wrote ? now && strcmp(now, wrote) == 0 : now == NULL)) && claim);
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
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    static const char *const libraries[] = { "lib_system", "lib_pairs", "lib_string" };
    for (size_t i = 0; i < 3; i++)
        require("import", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", libraries[i])))));
    const char *absent = "NO_SUCH_VARIABLE_HERE", *name = "METTA_SYSTEM_EXAMPLE";
    require("the variables start unset", !getenv(absent) && !getenv(name));

    /* Unset has no answer; a write is what getenv reads next. */
    assert(answers_are(mt_eval(m, E("collapse", E("env-get", T(absent)))), E(value_of_env(absent))) && "unset");
    assert(answers_are(mt_eval(m, E("if", E("==", mt_unit(), E("collapse", E("env-get", T(absent)))), "unset", "set")), E(S(getenv(absent) ? "set" : "unset")))
           && "a presence test");
    check_write("env-set!", m, E("env-set!", T(name), T("on")), name, "on");
    assert(answers_are(mt_eval(m, E("env-get", T(name))), E(T(getenv(name)))) && "which getenv reads");
    check_write("an empty value", m, E("env-set!", T(name), T("")), name, "");
    assert(answers_are(mt_eval(m, E("env-get", T(name))), E(T(getenv(name)))) && "is set");
    assert(answers_are(mt_eval(m, E("if", E("==", mt_unit(), E("collapse", E("env-get", T(name)))), "unset", "set")), E(S(getenv(name) ? "set" : "unset")))
           && "and present");
    check_write("env-unset!", m, E("env-unset!", T(name)), name, NULL);
    assert(answers_are(mt_eval(m, E("collapse", E("env-get", T(name)))), E(value_of_env(name))) && "then it is gone");
    check_write("removing it again is silent", m, E("env-unset!", T(name)), name, NULL);

    /* The whole environment, as environ holds it. */
    check_write("set to list", m, E("env-set!", T(name), T("listed")), name, "listed");
    assert(answers_are(mt_eval(m, E("pairs-is", E("env-all"))), E(B(relation_of_environ()))) && "the environment is a relation");
    assert(answers_are(mt_eval(m, E("collapse", E("pairs-lookup", E("env-all"), T(name)))), E(value_of_env(name))) && "which holds the value");
    assert(answers_are(mt_eval(m, E("collapse", E("pairs-lookup", E("env-all"), T(absent)))), E(value_of_env(absent))) && "and nothing unset");
    check_write("cleaned up", m, E("env-unset!", T(name)), name, NULL);

    /* The platform. */
    assert(answers_are(mt_eval(m, E("is-member", E("platform-info", "family"),
                                    mt_arrayv(sizeof families / sizeof *families, families, sizeof *families, family_text))), E(B(listed(families, sizeof families / sizeof *families, family()))))
           && "the family is one a platform flag names");
    assert(answers_are(mt_eval(m, E("platform-info", "dialect")), E(T("swi"))) && "the dialect cmetta embeds");
    assert(answers_are(mt_eval(m, E("platform-info", "cores")), E((int64_t)sysconf(_SC_NPROCESSORS_ONLN))) && "the cores are the processors online");
    mt_atom *version = value_of(m, E("platform-info", "version")), *numbers = value_of(m, E("platform-info", "version-numbers"));
    char copy[64], *save;
    size_t parts = 0;
    snprintf(copy, sizeof copy, "%s", mt_name(version));
    for (char *p = strtok_r(copy, ".", &save); p; p = strtok_r(NULL, ".", &save)) parts++;
    assert(answers_are(mt_eval(m, E("size-atom", E("platform-info", "version-numbers"))), E((int64_t)parts)) && "a version number per part of its text");
    assert(answers_are(mt_eval(m, E("size-atom", E("platform-keys"))), E((int64_t)(sizeof keys / sizeof *keys))) && "every key");
    assert(answers_are(mt_eval(m, guarded(E("platform-info", "nosuch"))), E(verdict(listed(keys, sizeof keys / sizeof *keys, "nosuch")))) && "an unknown key is refused");
    char major[24];
    snprintf(major, sizeof major, "%" PRId64, mt_int(mt_at(numbers, 0)));
    assert(answers_are(mt_eval(m, E("string-starts-with", E("platform-info", "version"),
                                    E("number-to-string", E("car-atom", E("platform-info", "version-numbers"))))), E(B(strncmp(mt_name(version), major, strlen(major)) == 0)))
           && "the text starts with the major number");

    /* The working directory is getcwd's. */
    char cwd[4096];
    require("getcwd", getcwd(cwd, sizeof cwd) != NULL);
    size_t len = strlen(cwd);
    assert(answers_are(mt_eval(m, E("string-starts-with", E("working-directory"), T("/"))), E(B(cwd[0] == '/'))) && "absolute");
    assert(answers_are(mt_eval(m, E("string-ends-with", E("working-directory"), T("/"))), E(B(len > 1 && cwd[len - 1] == '/'))) && "without a trailing separator");
    const char *nowhere = "/no/such/directory/here";
    assert(answers_are(mt_eval(m, guarded(E("change-directory!", T(nowhere)))), E(verdict(is_dir(nowhere)))) && "no moving into what is not there");
    char after[4096];
    require("getcwd", getcwd(after, sizeof after) != NULL);
    assert(answers_are(mt_eval(m, E("string-starts-with", E("working-directory"), T("/"))), E(B(strcmp(after, cwd) == 0 && after[0] == '/'))) && "and the directory stays");

    /* Refusals. */
    mt_atom *seven = mt_num(7), *nosuch = E("nosuch");
    assert(answers_are(mt_eval(m, guarded(E("env-get", mt_keep(seven)))), E(verdict(mt_kind_of(seven) == MT_TEXT))) && "a number is no name");
    assert(answers_are(mt_eval(m, guarded(E("env-set!", T("A"), mt_keep(nosuch)))), E(verdict(mt_kind_of(nosuch) == MT_TEXT))) && "an expression is no value");
    assert(answers_are(mt_eval(m, guarded(E("change-directory!", mt_keep(seven)))), E(verdict(mt_kind_of(seven) == MT_TEXT))) && "a number is no directory");

    mt_atom *held[] = { version, numbers, seven, nosuch };
    for (size_t i = 0; i < sizeof held / sizeof *held; i++) mt_drop(held[i]);
    mt_close(m);
    return 0;
}
