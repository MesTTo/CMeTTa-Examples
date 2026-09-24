/* Purpose: the numeric operations, each held to the C library function it
 *   is. A unary operation is a row: its name, math.h's function, and the
 *   kind of number it answers, an integer for the roundings, a real for the
 *   square root and the trigonometry, a truth for the NaN and infinity
 *   tests, and the argument's own kind for abs. pow-math keeps its operands'
 *   kinds: two integers give C's integer power, and a base of the integer 1
 *   gives the integer 1 whatever the exponent, SWI's special case; otherwise
 *   it is math.h's pow. log-math is the logarithm to the base its first
 *   argument names, min-atom and max-atom are C's folds over its array, and
 *   the symbols inf and nan name math.h's INFINITY and NAN.
 * Guarantees: all twenty-four claims of the original hold [tested: make
 *   twins; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include <math.h>

typedef enum { WHOLE, REAL, TRUTH, SAME } kind;

typedef struct unary {
    const char *name;
    double (*c)(double);
    kind answers;
} unary;

static double is_nan(double x) { return isnan(x); }
static double is_inf(double x) { return isinf(x); }

static const unary SQRT = { "sqrt-math", sqrt, REAL }, ABS = { "abs-math", fabs, SAME },
                   TRUNC = { "trunc-math", trunc, WHOLE }, CEIL = { "ceil-math", ceil, WHOLE },
                   FLOOR = { "floor-math", floor, WHOLE }, ROUND = { "round-math", round, WHOLE },
                   SIN = { "sin-math", sin, REAL }, ASIN = { "asin-math", asin, REAL }, COS = { "cos-math", cos, REAL },
                   ACOS = { "acos-math", acos, REAL }, TAN = { "tan-math", tan, REAL }, ATAN = { "atan-math", atan, REAL },
                   ISNAN = { "isnan-math", is_nan, TRUTH }, ISINF = { "isinf-math", is_inf, TRUTH };

/* A number as the original writes it: an integer or a real. */
static mt_atom *number(double x, bool whole) { return whole ? N((int64_t)x) : R(x); }

/* What OP answers for X, as the kind of number its row says. */
static mt_atom *answer(const unary *op, double x, bool whole)
{
    double y = op->c(x);
    switch (op->answers) {
    case WHOLE: return N((int64_t)y);
    case REAL: return R(y);
    case TRUTH: return B(y != 0);
    default: return number(y, whole);
    }
}

/* pow-math as the engine answers it. C models an integer power only for a
   non-negative exponent, the one the original raises. */
static mt_atom *pow_math(double base, bool base_whole, double exponent, bool exponent_whole)
{
    if (base_whole && base == 1) return N(1);
    if (base_whole && exponent_whole) {
        require("a non-negative integer exponent", exponent >= 0);
        int64_t power = 1;
        for (int64_t i = 0; i < (int64_t)exponent; i++) power *= (int64_t)base;
        return N(power);
    }
    return R(pow(base, exponent));
}

static const int64_t items[] = { 2, 6, 7, 4, 9, 3 };
#define ITEMS (sizeof items / sizeof *items)

int main(void)
{
    metta *m = open_engine();
    check_answers("two integers give the integer power", mt_eval(m, E("pow-math", 2, 3)), pow_math(2, true, 3, true));
    check_answers("the square root of -1 is NaN", mt_eval(m, E(ISNAN.name, E(SQRT.name, -1))), B(isnan(sqrt(-1.0))));
    check_answers("a real zero to a negative power is infinite", mt_eval(m, E(ISINF.name, E("pow-math", 0.0, -1.0))),
                  B(isinf(pow(0.0, -1.0))));
    check_answers("an integer base of 1 keeps its kind", mt_eval(m, E("pow-math", 1, 2147483648.0)),
                  pow_math(1, true, 2147483648.0, false));

    static const struct { const unary *op; double x; bool whole; } asked[] = {
        { &SQRT, 9, true },  { &ABS, -5, true }, { &TRUNC, 5.6, false }, { &CEIL, 5.2, false }, { &FLOOR, 5.8, false },
        { &ROUND, 5.4, false }, { &ROUND, 5.6, false }, { &SIN, 0, true }, { &ASIN, 0, true }, { &COS, 0, true },
        { &ACOS, 1, true }, { &TAN, 0, true }, { &ATAN, 0, true }, { &ISNAN, 0.0, false }, { &ISINF, 0.0, false },
    };
    for (size_t i = 0; i < sizeof asked / sizeof *asked; i++)
        check_answers(asked[i].op->name, mt_eval(m, E(asked[i].op->name, number(asked[i].x, asked[i].whole))),
                      answer(asked[i].op, asked[i].x, asked[i].whole));
    check_answers("log-math takes its base first", mt_eval(m, E("log-math", 10, 100)), R(log(100.0) / log(10.0)));

    int64_t least = items[0], most = items[0];
    mt_atom *list[ITEMS];
    for (size_t i = 0; i < ITEMS; i++) {
        least = items[i] < least ? items[i] : least;
        most = items[i] > most ? items[i] : most;
        list[i] = N(items[i]);
    }
    mt_atom *all = mt_exprv(ITEMS, list);
    check_int("min-atom", mt_one_int(mt_eval(m, E("min-atom", mt_keep(all)))), least);
    check_int("max-atom", mt_one_int(mt_eval(m, E("max-atom", mt_keep(all)))), most);
    mt_drop(all);

    static const struct { const unary *op; const char *symbol; double value; } named[] = {
        { &ISINF, "inf", INFINITY }, { &ISNAN, "nan", NAN },
    };
    for (size_t i = 0; i < sizeof named / sizeof *named; i++)
        check_answers("a symbol naming a special real", mt_eval(m, E(named[i].op->name, named[i].symbol)),
                      answer(named[i].op, named[i].value, false));
    return done(m);
}
