/* Purpose: the # operators are CLP(FD) constraints, so one table serves both
 *   directions. Each row is an operation, its two operands and C's own
 *   operator on them: forwards the engine agrees with C, and backwards
 *   mt_solve hides the first operand and recovers it by name. The division,
 *   extremes and comparisons tables carry C's operator the same way, and the
 *   composed query solves through two constraints.
 * Guarantees: all twenty claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

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
    metta *m = open_engine();

    for (size_t i = 0; i < sizeof FORWARD / sizeof *FORWARD; i++)
        check_answers(FORWARD[i].op, mt_eval(m, E(FORWARD[i].op, FORWARD[i].a, FORWARD[i].b)), FORWARD[i].c);
    for (size_t i = 0; i < sizeof BACKWARD / sizeof *BACKWARD; i++)
        check_int(BACKWARD[i].op, solved(mt_solve(m, N(BACKWARD[i].c),
                                                  E(BACKWARD[i].op, V(BACKWARD[i].var), BACKWARD[i].b)),
                                         BACKWARD[i].var),
                  BACKWARD[i].want);
    for (size_t i = 0; i < sizeof ARITH / sizeof *ARITH; i++)
        check_answers(ARITH[i].op, mt_eval(m, E(ARITH[i].op, ARITH[i].a, ARITH[i].b)), ARITH[i].c);
    for (size_t i = 0; i < sizeof COMPARE / sizeof *COMPARE; i++)
        check_answers(COMPARE[i].op, mt_eval(m, E(COMPARE[i].op, COMPARE[i].a, COMPARE[i].b)),
                      B(COMPARE[i].c));
    check_int("composed, solved backwards through two constraints",
              solved(mt_solve(m, N(20), E("#*", E("#+", V("a"), 1), 4)), "a"), 4);
    return done(m);
}
