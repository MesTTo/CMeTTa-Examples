/* Purpose: one body, two languages. POLY is written once over its operators;
 *   expanded with the C_ operators it is the C function poly(),
 *   expanded with its atom builders it is the equation mt_add() installs, and
 *   the two agree on every input tried. The equation is visible to the engine
 *   as data, which a C function published with mt_def() would not be. The
 *   operators agree only where their spellings do, and % is where C and
 *   MeTTa part: WRAP builds MeTTa's %, and C_MOD agrees with it on every
 *   combination of signs, where C's own % would not. The float operators get
 *   the same grid: /, a zero divisor included, min, max, <= and and.
 * Guarantees: C and the equation agree on -20..20, the equation is stored as
 *   an atom a match can find, C_MOD is MeTTa's % over -7..7 by -3, -2, 2 and
 *   3, and C_DIV, C_MIN, C_MAX, C_LE and C_AND are MeTTa's over every pair
 *   of -2.5, -1.0, 0.0, 0.5 and 3.0 but zero over zero
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>

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

/* The remainder whose sign is the divisor's, which is what MeTTa's %
   answers, where C's % takes the dividend's sign. */
static inline int64_t floor_mod(int64_t a, int64_t b)
{
    int64_t r = a % b;
    return r != 0 && (r < 0) != (b < 0) ? r + b : r;
}

/* Each MeTTa operator twice, for a body written once over its operators:
   C_X computes in C, and T_X builds the atom (X a b). */
#define C_LE(a, b) ((a) <= (b))
#define T_LE(a, b) mt_expr("<=", a, b)
#define C_ADD(a, b) ((a) + (b))
#define T_ADD(a, b) mt_expr("+", a, b)
#define C_SUB(a, b) ((a) - (b))
#define T_SUB(a, b) mt_expr("-", a, b)
#define C_MUL(a, b) ((a) * (b))
#define T_MUL(a, b) mt_expr("*", a, b)
#define C_DIV(a, b) ((a) / (b))
#define T_DIV(a, b) mt_expr("/", a, b)
#define C_MOD(a, b) floor_mod(a, b)
#define T_MOD(a, b) mt_expr("%", a, b)
#define C_MIN(a, b) ((a) < (b) ? (a) : (b))
#define T_MIN(a, b) mt_expr("min", a, b)
#define C_MAX(a, b) ((a) > (b) ? (a) : (b))
#define T_MAX(a, b) mt_expr("max", a, b)
#define C_AND(a, b) ((a) && (b))
#define T_AND(a, b) mt_expr("and", a, b)

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
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("lower the shared body", mt_add(m, E("=", E("poly", V("x")), POLY(T_ADD, T_MUL, V("x")))));
    bool agree = true;
    for (int64_t x = -20; x <= 20 && agree; x++)
        agree = mt_one_int(mt_eval(m, E("poly", x))) == poly(x);
    assert(agree && "C and the equation agree on -20..20");
    assert(answers_are(mt_match(m, E("=", E("poly", V("x")), V("body"))), E(E("=", E("poly", V("x")), E("+", E("*", 3, V("x")), 1))))
           && "the equation is an atom the space holds");

    require("lower the remainder", mt_add(m, E("=", E("wrap", V("x"), V("n")), WRAP(T_MOD, V("x"), V("n")))));
    static const int64_t divisors[] = { -3, -2, 2, 3 };
    bool same_sign_rule = true;
    for (int64_t x = -7; x <= 7; x++)
        for (size_t i = 0; i < sizeof divisors / sizeof *divisors; i++)
            same_sign_rule = same_sign_rule && mt_one_int(mt_eval(m, E("wrap", x, divisors[i]))) == wrap(x, divisors[i]);
    assert(same_sign_rule && "C_MOD is MeTTa's % for every combination of signs");
    assert(mt_one_int(mt_eval(m, E("wrap", -7, 3))) == 2 && "(% -7 3) is 2, where C's -7 % 3 is -1");

    require("lower the ratio", mt_add(m, E("=", E("ratio", V("x"), V("y")), RATIO(T_DIV, V("x"), V("y")))));
    require("lower the span", mt_add(m, E("=", E("span", V("x"), V("y")), SPAN(T_SUB, T_MIN, T_MAX, V("x"), V("y")))));
    require("lower the bounds test",
            mt_add(m, E("=", E("within", V("lo"), V("x"), V("hi")), WITHIN(T_AND, T_LE, V("lo"), V("x"), V("hi")))));
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
    assert(divides && "C_DIV is MeTTa's / on floats, a zero divisor's infinity included");
    assert(spans && "C_MIN and C_MAX are MeTTa's min and max");
    assert(bounds && "C_AND and C_LE are MeTTa's and and <=");
    mt_close(m);
    return 0;
}
