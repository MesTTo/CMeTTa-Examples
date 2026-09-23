/* Purpose: prove claims whatever NDEBUG says, and report what the twin lane
 *   compares: the claims a program proved, the definitions and C operations
 *   it made visible, and the atoms its &self holds.
 * Owns resources: done() closes the runtime; every check that takes an atom or
 *   a cursor releases it on every path. A failed claim ends the process.
 * Guarded by: the claim counter is atomic, because worker threads prove
 *   claims too; everything else runs on the thread that calls done().
 * Guarantees: the space report is the same function for a twin and for the
 *   original's runner, tools/original.c, so the two sides of the lane cannot
 *   canonicalise atoms two ways [tested: make twins; commit=WORKTREE].
 */
#include "common.h"
#include "lane.h"
#include <inttypes.h>
#include <stdarg.h>
#include <stdatomic.h>

static atomic_size_t claims;
static uint64_t inferences_at_open;

static void fail(const char *claim, const char *detail, ...)
{
    va_list args;
    fprintf(stderr, "FAIL %s", claim);
    if (detail) {
        fputs(": ", stderr);
        va_start(args, detail);
        vfprintf(stderr, detail, args);
        va_end(args);
    }
    fputc('\n', stderr);
    if (mt_errmsg()) fprintf(stderr, "  engine: %s (%s)\n", mt_errmsg(),
                             mt_status_str(mt_error()));
    exit(EXIT_FAILURE);
}

static void proved(void) { atomic_fetch_add(&claims, 1); }

metta *open_engine(void)
{
    mt_clear();
    metta *runtime = mt_open(NULL);
    if (!runtime) fail("open the engine", NULL);
    inferences_at_open = mt_stats_now(runtime).inferences;
    return runtime;
}

void require(const char *what, bool ok)
{
    if (!ok) fail(what, "a door the program needs refused");
}

void check(const char *claim, bool holds)
{
    if (!holds) fail(claim, NULL);
    proved();
}

void check_int(const char *claim, int64_t got, int64_t want)
{
    if (got != want)
        fail(claim, "got %" PRId64 ", want %" PRId64, got, want);
    proved();
}

void check_real(const char *claim, double got, double want)
{
    if (!(got == want)) fail(claim, "got %.17g, want %.17g", got, want);
    proved();
}

void check_text(const char *claim, const char *got, const char *want)
{
    if (!got || strcmp(got, want) != 0)
        fail(claim, "got \"%s\", want \"%s\"", got ? got : "(null)", want);
    proved();
}

void check_atom(const char *claim, mt_atom *got, mt_atom *want)
{
    bool holds = got && want && mt_alpha_eq(got, want);
    if (!holds) {
        char *g = mt_show_dup(got), *w = mt_show_dup(want);
        mt_drop(got);
        mt_drop(want);
        fail(claim, "got %s, want %s", g ? g : "(null)", w ? w : "(null)");
    }
    mt_drop(got);
    mt_drop(want);
    proved();
}

/* Time: sum of the answers' sizes for the walk, plus one alpha comparison per
   expected answer. */
void check_answers_(const char *claim, mt_answers *answers, size_t count,
                    mt_atom **want)
{
    check_list_(claim, mt_all(answers), count, want);
}

void check_list_(const char *claim, mt_list got, size_t count, mt_atom **want)
{
    bool holds = mt_ok() && got.len == count;
    for (size_t i = 0; holds && i < count; i++)
        holds = want[i] && mt_alpha_eq(got.items[i], want[i]);
    if (!holds) {
        fprintf(stderr, "  got %zu answer(s):", got.len);
        for (size_t i = 0; i < got.len; i++)
            fprintf(stderr, " %s", mt_show(got.items[i]));
        fprintf(stderr, "\n  want %zu:", count);
        for (size_t i = 0; i < count; i++)
            fprintf(stderr, " %s", want[i] ? mt_show(want[i]) : "(null)");
        fputc('\n', stderr);
    }
    mt_list_free(got);
    for (size_t i = 0; i < count; i++) mt_drop(want[i]);
    if (!holds) fail(claim, "the answers differ");
    proved();
}

void check_none(const char *claim, mt_answers *answers)
{
    mt_list got = mt_all(answers);
    bool holds = mt_ok() && got.len == 0;
    if (!holds && got.len)
        fprintf(stderr, "  first of %zu: %s\n", got.len, mt_show(got.items[0]));
    mt_list_free(got);
    if (!holds) fail(claim, "wanted no answer");
    proved();
}

int done_(metta *runtime, const char *file)
{
    size_t count = atomic_load(&claims);
    uint64_t spent = runtime ? mt_stats_now(runtime).inferences - inferences_at_open : 0;
    if (count == 0) fail("prove at least one claim", "%s checked nothing", file);
    if (!mt_ok()) fail("leave no error unhandled", NULL);
    printf("LANE-CLAIMS %zu\n", count);
    printf("LANE-INFERENCES %" PRIu64 "\n", spent);
    if (runtime) {
        lane_report(runtime);
        mt_close(runtime);
        if (!mt_ok()) fail("close the engine", NULL);
    }
    printf("OK %s (%zu claims)\n", file, count);
    return EXIT_SUCCESS;
}
