/* Purpose: one body, two languages. POLY is written once over its operators;
 *   expanded with lowering.h's C operators it is the C function poly(),
 *   expanded with MeTTa's it is the equation mt_lower() installs, and the two
 *   agree on every input tried. The equation is visible to the engine as
 *   data, which a C function published with mt_def() would not be. The
 *   operators agree only where their spellings do, and % is where C and
 *   MeTTa part: WRAP lowers MeTTa's %, and C_MOD agrees with it on every
 *   combination of signs, where C's own % would not. The float operators get
 *   the same grid: /, a zero divisor included, min, max, <= and and.
 * Guarantees: C and the equation agree on -20..20, the equation is stored as
 *   an atom a match can find, C_MOD is MeTTa's % over -7..7 by -3, -2, 2 and
 *   3, and C_DIV, C_MIN, C_MAX, C_LE and C_AND are MeTTa's over every pair
 *   of -2.5, -1.0, 0.0, 0.5 and 3.0 but zero over zero [tested: make check;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"
#include "lowering.h"

#define POLY(ADD, MUL, x) ADD(MUL(3, x), 1)
#define WRAP(MOD, x, n) MOD(x, n)
#define RATIO(DIV, x, y) DIV(x, y)
#define SPAN(SUB, MIN, MAX, x, y) SUB(MAX(x, y), MIN(x, y))
#define WITHIN(AND, LE, lo, x, hi) AND(LE(lo, x), LE(x, hi))

static int64_t poly(int64_t x) { return POLY(C_ADD, C_MUL, x); }
static int64_t wrap(int64_t x, int64_t n) { return WRAP(C_MOD, x, n); }
static double ratio(double x, double y) { return RATIO(C_DIV, x, y); }
static double span(double x, double y) { return SPAN(C_SUB, C_MIN, C_MAX, x, y); }
static bool within(double lo, double x, double hi) { return WITHIN(C_AND, C_LE, lo, x, hi); }

int main(void)
{
    metta *m = open_engine();
    require("lower the shared body", mt_lower(m, (poly $x), POLY(M_ADD, M_MUL, $x)));
    bool agree = true;
    for (int64_t x = -20; x <= 20 && agree; x++)
        agree = mt_one_int(mt_eval(m, E("poly", x))) == poly(x);
    check("C and the equation agree on -20..20", agree);
    check_answers("the equation is an atom the space holds",
                  mt_match(m, E("=", E("poly", V("x")), V("body"))),
                  E("=", E("poly", V("x")), E("+", E("*", 3, V("x")), 1)));

    require("lower the remainder", mt_lower(m, (wrap $x $n), WRAP(M_MOD, $x, $n)));
    static const int64_t divisors[] = { -3, -2, 2, 3 };
    bool same_sign_rule = true;
    for (int64_t x = -7; x <= 7; x++)
        for (size_t i = 0; i < sizeof divisors / sizeof *divisors; i++)
            same_sign_rule = same_sign_rule && mt_one_int(mt_eval(m, E("wrap", x, divisors[i]))) == wrap(x, divisors[i]);
    check("C_MOD is MeTTa's % for every combination of signs", same_sign_rule);
    check_int("(% -7 3) is 2, where C's -7 % 3 is -1", mt_one_int(mt_eval(m, E("wrap", -7, 3))), 2);

    require("lower the ratio", mt_lower(m, (ratio $x $y), RATIO(M_DIV, $x, $y)));
    require("lower the span", mt_lower(m, (span $x $y), SPAN(M_SUB, M_MIN, M_MAX, $x, $y)));
    require("lower the bounds test", mt_lower(m, (within $lo $x $hi), WITHIN(M_AND, M_LE, $lo, $x, $hi)));
    static const double grid[] = { -2.5, -1.0, 0.0, 0.5, 3.0 };
    enum { GRID = sizeof grid / sizeof *grid };
    bool divides = true, spans = true, bounds = true;
    for (size_t i = 0; i < GRID; i++)
        for (size_t j = 0; j < GRID; j++) {
            double x = grid[i], y = grid[j];
            if (x != 0.0 || y != 0.0) divides = divides && mt_one_float(mt_eval(m, E("ratio", x, y))) == ratio(x, y);
            spans = spans && mt_one_float(mt_eval(m, E("span", x, y))) == span(x, y);
            for (size_t k = 0; k < GRID; k++) bounds = bounds && mt_one_truth(mt_eval(m, E("within", x, grid[k], y))) == within(x, grid[k], y);
        }
    check("C_DIV is MeTTa's / on floats, a zero divisor's infinity included", divides);
    check("C_MIN and C_MAX are MeTTa's min and max", spans);
    check("C_AND and C_LE are MeTTa's and and <=", bounds);
    return done(m);
}
