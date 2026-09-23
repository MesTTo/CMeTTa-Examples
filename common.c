/* Purpose: fail on wrong results even when NDEBUG disables C assert.
 * Owns resources: check_answers consumes its cursor and parsed expectations;
 *   done closes the runtime. A failed check terminates the example process.
 * Guarded by: the assertion counter is atomic for joined worker examples.
 * Guarantees: success requires a checked result [tested: make check-helpers; commit=WORKTREE].
 * Open Obligations: None.
 */
#include "common.h"
#include <stdatomic.h>

static atomic_size_t checks;

void check(const char *label, bool condition)
{
    if (!condition) {
        fprintf(stderr, "FAIL %s: %s (%s)\n", label,
                mt_errmsg() ? mt_errmsg() : "unexpected result",
                mt_status_str(mt_error()));
        exit(EXIT_FAILURE);
    }
    atomic_fetch_add(&checks, 1);
}

void check_atom(const char *label, const mt_atom *actual, const char *expected)
{
    mt_atom *wanted = mt_parse(expected);
    check(label, actual && wanted && mt_eq(actual, wanted) && mt_ok());
    mt_drop(wanted);
}

void check_answers(const char *label, mt_answers *answers, const char *expected)
{
    check("open answer cursor", answers != NULL);
    mt_list wanted = mt_forms(expected);
    mt_list actual = mt_all(answers);
    check(label, mt_ok() && actual.len == wanted.len);
    for (size_t i = 0; i < actual.len; ++i) {
        if (!mt_eq(actual.items[i], wanted.items[i])) {
            fprintf(stderr, "%s row %zu: got %s; expected %s\n", label, i,
                    mt_show(actual.items[i]), mt_show(wanted.items[i]));
            check(label, false);
        }
    }
    check(label, mt_ok());
    mt_list_free(actual);
    mt_list_free(wanted);
}

metta *open_engine(void)
{
    mt_clear();
    metta *runtime = mt_open(NULL);
    if (!runtime) check("open embedded engine", false);
    return runtime;
}

/* Join source fragments before one run so program order and source identity
 * retain the engine's semantics. Time and space: O(B), B source bytes.
 */
void check_program(metta *runtime, const char *const *fragments, size_t count)
{
    size_t bytes = 1;
    for (size_t i = 0; i < count; ++i) {
        size_t length = strlen(fragments[i]);
        check("source size fits", length <= SIZE_MAX - bytes);
        bytes += length;
    }
    char *source = malloc(bytes);
    check("allocate source", source != NULL);
    size_t at = 0;
    for (size_t i = 0; i < count; ++i) {
        size_t length = strlen(fragments[i]);
        memcpy(source + at, fragments[i], length); at += length;
    }
    source[at] = '\0';
    bool success = mt_do(runtime, source);
    free(source);
    check("all embedded result assertions", success && mt_ok());
}

int done(metta *runtime, const char *name)
{
    size_t count = atomic_load(&checks);
    check("at least one result was checked", count > 0);
    check("no unhandled error", mt_ok());
    mt_close(runtime);
    check("engine closed", mt_ok());
    printf("OK %s (%zu checks)\n", name, count);
    return EXIT_SUCCESS;
}
