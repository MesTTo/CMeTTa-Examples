/* Purpose: the # operators are CLP(FD) constraints, so one table serves both
 *   directions. Each row is an operation, its two operands and C's own
 *   operator on them: forwards the engine agrees with C, and backwards
 *   mt_solve hides the first operand and recovers it by name. The division,
 *   extremes and comparisons tables carry C's operator the same way, and the
 *   composed query solves through two constraints.
 * Guarantees: all twenty claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>

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

static const struct { const char *op; int64_t a, b, c; } FORWARD[] = {
    { "#+", 1, 2, 1 + 2 }, { "#*", 3, 4, 3 * 4 }, { "#-", 10, 4, 10 - 4 },
};

/* c = (op $var b), solved for $var. */
static const struct { const char *op, *var; int64_t b, c, want; } BACKWARD[] = {
    { "#+", "x", 2, 5, 3 }, { "#*", "y", 4, 12, 3 }, { "#-", "z", 4, 6, 10 },
};

static const struct { const char *op; int64_t a, b, c; } ARITH[] = {
    { "#div", 13, 4, 13 / 4 },         { "#mod", 13, 4, 13 % 4 },
    { "#min", 3, 7, 3 < 7 ? 3 : 7 },   { "#max", 3, 7, 3 > 7 ? 3 : 7 },
};

static const struct { const char *op; int64_t a, b; bool c; } COMPARE[] = {
    { "#<", 1, 2, 1 < 2 },   { "#<", 2, 1, 2 < 1 },    { "#>", 2, 1, 2 > 1 },
    { "#=", 3, 3, 3 == 3 },  { "#\\=", 3, 4, 3 != 4 }, { "#=<", 1, 2, 1 <= 2 },
    { "#=<", 2, 1, 2 <= 1 }, { "#>=", 2, 1, 2 >= 1 },  { "#>=", 1, 2, 1 >= 2 },
};

/* The one binding a solve answers for `name`, or a value no row can hold. */
static int64_t solved(mt_answers *rows, const char *name)
{
    int64_t value = INT64_MIN;
    size_t count = 0;
    mt_rows (row, rows) {
        value = mt_int(mt_bound(row, name));
        count++;
    }
    return count == 1 ? value : INT64_MIN;
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;

    for (size_t i = 0; i < sizeof FORWARD / sizeof *FORWARD; i++)
        assert(answers_are(mt_eval(m, E(FORWARD[i].op, FORWARD[i].a, FORWARD[i].b)), E(FORWARD[i].c)) && FORWARD[i].op);
    for (size_t i = 0; i < sizeof BACKWARD / sizeof *BACKWARD; i++)
        assert(solved(mt_solve(m, N(BACKWARD[i].c),
                               E(BACKWARD[i].op, V(BACKWARD[i].var), BACKWARD[i].b)),
                      BACKWARD[i].var) == BACKWARD[i].want
               && BACKWARD[i].op);
    for (size_t i = 0; i < sizeof ARITH / sizeof *ARITH; i++)
        assert(answers_are(mt_eval(m, E(ARITH[i].op, ARITH[i].a, ARITH[i].b)), E(ARITH[i].c)) && ARITH[i].op);
    for (size_t i = 0; i < sizeof COMPARE / sizeof *COMPARE; i++)
        assert(answers_are(mt_eval(m, E(COMPARE[i].op, COMPARE[i].a, COMPARE[i].b)), E(B(COMPARE[i].c)))
               && COMPARE[i].op);
    assert(solved(mt_solve(m, N(20), E("#*", E("#+", V("a"), 1), 4)), "a") == 4
           && "composed, solved backwards through two constraints");
    mt_close(m);
    return 0;
}
