/* Purpose: prove claims whatever NDEBUG says, and report what the twin lane
 *   compares: the claims a program proved, the definitions and C operations
 *   it made visible, and the atoms its &self holds.
 * Owns resources: done() closes the runtime; every check that takes an atom or
 *   a cursor releases it on every path. A failed claim ends the process.
 *   open_engine() gives cmetta a counting allocator on its thread, so done()
 *   can see what the program made and never released.
 * Guarded by: the claim counter and the held-block counts are atomic,
 *   because worker threads prove claims and release atoms too; everything
 *   else runs on the thread that calls done().
 * Guarantees: the space report is the same function for a twin and for the
 *   original's runner, tools/original.c, so the two sides of the lane cannot
 *   canonicalise atoms two ways [tested: make twins; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#include "common.h"
#include "lane.h"
#include <inttypes.h>
#include <stdarg.h>
#include <stdatomic.h>

static atomic_size_t claims;
static uint64_t inferences_at_open;

/* The blocks cmetta allocated on the thread that opened the engine and has
   not yet released. A block goes back through the allocator that made it,
   on whatever thread releases it, so the count is exact whoever drops an
   atom last. */
static atomic_size_t held_blocks, held_bytes;

static void *counted_resize(void *user, void *pointer, size_t old_size, size_t new_size)
{
    (void)user;
    if (new_size == 0) {
        atomic_fetch_sub(&held_blocks, 1);
        atomic_fetch_sub(&held_bytes, old_size);
        free(pointer);
        return NULL;
    }
    void *block = realloc(pointer, new_size);
    if (block) {
        if (old_size == 0) atomic_fetch_add(&held_blocks, 1);
        atomic_fetch_add(&held_bytes, new_size);
        atomic_fetch_sub(&held_bytes, old_size);
    }
    return block;
}

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

mt_atom *bag_minus(const mt_atom *from, const mt_atom *take)
{
    size_t n = mt_len(from), m = mt_len(take), kept = 0;
    bool *spent = calloc(m + 1, sizeof *spent);
    mt_atom **left = malloc((n + 1) * sizeof *left);
    require("room for a bag", spent && left);
    for (size_t i = 0; i < n; i++) {
        size_t j = 0;
        while (j < m && (spent[j] || mt_compare(mt_at(from, i), mt_at(take, j)) != 0)) j++;
        if (j < m) spent[j] = true;
        else left[kept++] = mt_keep(mt_at(from, i));
    }
    mt_atom *out = mt_exprv(kept, left);
    free(spent), free(left);
    return out;
}

const char *metatype(const mt_atom *atom)
{
    switch (mt_kind_of(atom)) {
    case MT_SYMBOL:
    case MT_SPACE: return "Symbol";
    case MT_VARIABLE: return "Variable";
    case MT_EXPR: return "Expression";
    default: return "Grounded";
    }
}

metta *open_engine(void)
{
    mt_clear();
    (void)mt_allocator_set((mt_allocator){ counted_resize, NULL });
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

bool alpha_equal(const mt_atom *got, mt_atom *want)
{
    bool equal = got && want && mt_alpha_eq(got, want);
    mt_drop(want);
    return equal;
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

void check_value(const char *claim, mt_answers *answers, mt_atom *value)
{
    mt_atom *empty = mt_sym("Empty");
    bool nothing = value && mt_eq(value, empty);
    mt_drop(empty);
    if (nothing) {
        mt_drop(value);
        check_none(claim, answers);
    } else
        check_answers_(claim, answers, 1, &value);
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
        /* A pipe buffers stdout, and a close that crashes discards the
           buffer: SWI's halt can race a thread the engine created just
           before (ERRORS.md, Open). Flushed first, the claims and the space
           report reach the lane whatever the close does. */
        fflush(stdout);
        mt_close(runtime);
        if (!mt_ok()) fail("close the engine", NULL);
    }
    size_t blocks = atomic_load(&held_blocks);
    if (blocks) fail("release every atom", "%zu blocks (%zu bytes) outlived the engine", blocks, atomic_load(&held_bytes));
    printf("OK %s (%zu claims)\n", file, count);
    return EXIT_SUCCESS;
}
