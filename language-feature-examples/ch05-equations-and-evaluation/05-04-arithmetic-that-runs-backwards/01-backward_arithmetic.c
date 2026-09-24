/* Purpose: + - * / are relations, and C asks for an inverse with mt_solve,
 *   which puts the known value on let's pattern side and answers the unknowns
 *   by name. double and square are equations C builds, and they run
 *   backwards for free; each operator solves its one unknown from a table; no
 *   integer doubles to 7, so that solve has no rows; square and (* $x $y)
 *   post a CLP(FD) constraint and label every solution, the factor pairs of
 *   25 read with mt_bound against a C table; a bound posted first is a guard
 *   term, (let True bound question); and the # family posts rather than
 *   solves, so the composed query the ordinary operators refuse answers. The
 *   # tables carry C's own operation in their last column, floored as
 *   CLP(FD)'s div and mod are.
 * Guarantees: all twenty-five claims of the original hold [tested: make
 *   twins; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include "lowering.h"

/* (let True cond answer): answer only where cond holds, asked in the same
   derivation, so a constraint cond posts is still in force for answer. */
#define WHERE(cond, answer) E("let", B(true), (cond), (answer))

/* known = (op $var operand), solved for $var. */
static const struct { int64_t known; const char *op, *var; int64_t operand, want; } INVERSE[] = {
    { 5, "+", "p", 2, 3 }, { 12, "*", "q", 4, 3 }, { 6, "-", "r", 4, 10 }, { 3, "/", "s", 4, 12 },
};

/* CLP(FD)'s div and mod floor, as Prolog's do, where C's / and % truncate. */
static int64_t floor_div(int64_t a, int64_t b) { return (a - floor_mod(a, b)) / b; }
static int64_t smaller(int64_t a, int64_t b) { return C_MIN(a, b); }
static int64_t larger(int64_t a, int64_t b) { return C_MAX(a, b); }

static const struct { const char *op; int64_t a, b; int64_t (*c)(int64_t, int64_t); } ARITH[] = {
    { "#div", 13, 4, floor_div }, { "#mod", 13, 4, floor_mod },
    { "#min", 3, 7, smaller },    { "#max", 3, 7, larger },
};

static const struct { const char *op; int64_t a, b; bool c; } COMPARE[] = {
    { "#<", 1, 2, 1 < 2 },   { "#<", 2, 1, 2 < 1 },    { "#>", 2, 1, 2 > 1 },
    { "#=", 3, 3, 3 == 3 },  { "#\\=", 3, 4, 3 != 4 }, { "#=<", 1, 2, 1 <= 2 },
    { "#=<", 2, 1, 2 <= 1 }, { "#>=", 2, 1, 2 >= 1 },  { "#>=", 1, 2, 1 >= 2 },
};

/* The factor pairs of 25, in the order labelling finds them. */
static const int64_t FACTORS[][2] = { {-25, -1}, {-5, -5}, {-1, -25}, {1, 25}, {5, 5}, {25, 1} };

static mt_atom *composed(void) { return E("#*", E("#+", V("a"), 1), 4); }

int main(void)
{
    metta *m = open_engine();
    require("(= (double $x) (* 2 $x))", mt_add(m, E("=", E("double", V("x")), E("*", 2, V("x")))));
    require("(= (square $x) (* $x $x))", mt_add(m, E("=", E("square", V("x")), E("*", V("x"), V("x")))));

    check_answers("(double 5)", mt_eval(m, E("double", 5)), 10);
    check_list("double runs backwards", mt_all(mt_solve(m, N(10), E("double", V("x")))), 5);
    for (size_t i = 0; i < sizeof INVERSE / sizeof *INVERSE; i++)
        check_list(INVERSE[i].op, mt_all(mt_solve(m, N(INVERSE[i].known),
                                                  E(INVERSE[i].op, V(INVERSE[i].var), INVERSE[i].operand))),
                   INVERSE[i].want);
    check_none("no integer doubles to 7", mt_solve(m, N(7), E("double", V("x"))));

    check_list("both roots of 25", mt_all(mt_solve(m, N(25), E("square", V("x")))), -5, 5);
    size_t rows = 0, matched = 0;
    mt_rows (row, mt_solve(m, N(25), E("*", V("x"), V("y")))) {
        matched += rows < 6 && mt_int(mt_bound(row, "x")) == FACTORS[rows][0] &&
                   mt_int(mt_bound(row, "y")) == FACTORS[rows][1];
        rows++;
    }
    check("every factor pair of 25, in order, read by name", rows == 6 && matched == 6);

    check_answers("a bound posted first decides the root",
                  mt_eval(m, WHERE(E("#>=", V("x"), 0), E("let", 25, E("square", V("x")), V("x")))), 5);
    check_list("the # family solves the composed query", mt_all(mt_solve(m, N(20), composed())), 4);

    for (size_t i = 0; i < sizeof ARITH / sizeof *ARITH; i++)
        check_answers(ARITH[i].op, mt_eval(m, E(ARITH[i].op, ARITH[i].a, ARITH[i].b)), ARITH[i].c(ARITH[i].a, ARITH[i].b));
    for (size_t i = 0; i < sizeof COMPARE / sizeof *COMPARE; i++)
        check_answers(COMPARE[i].op, mt_eval(m, E(COMPARE[i].op, COMPARE[i].a, COMPARE[i].b)),
                      B(COMPARE[i].c));

    int64_t a = 0;
    mt_rows (row, mt_solve(m, N(20), composed())) a = mt_int(mt_bound(row, "a"));
    check_int("and read by name, through two constraints", a, 4);
    return done(m);
}
