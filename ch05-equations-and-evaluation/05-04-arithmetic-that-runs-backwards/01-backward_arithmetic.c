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
 * Guarantees: all twenty-five claims of the original hold
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
#define C_MIN(a, b) ((a) < (b) ? (a) : (b))
#define C_MAX(a, b) ((a) > (b) ? (a) : (b))

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
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("(= (double $x) (* 2 $x))", mt_add(m, E("=", E("double", V("x")), E("*", 2, V("x")))));
    require("(= (square $x) (* $x $x))", mt_add(m, E("=", E("square", V("x")), E("*", V("x"), V("x")))));

    assert(answers_are(mt_eval(m, E("double", 5)), E(10)) && "(double 5)");
    assert(list_is(mt_all(mt_solve(m, N(10), E("double", V("x")))), E(5)) && "double runs backwards");
    for (size_t i = 0; i < sizeof INVERSE / sizeof *INVERSE; i++)
        assert(list_is(mt_all(mt_solve(m, N(INVERSE[i].known),
                                       E(INVERSE[i].op, V(INVERSE[i].var), INVERSE[i].operand))), E(INVERSE[i].want))
               && INVERSE[i].op);
    assert(!mt_first(mt_solve(m, N(7), E("double", V("x")))) && mt_ok() && "no integer doubles to 7");

    assert(list_is(mt_all(mt_solve(m, N(25), E("square", V("x")))), E(-5, 5)) && "both roots of 25");
    size_t rows = 0, matched = 0;
    mt_rows (row, mt_solve(m, N(25), E("*", V("x"), V("y")))) {
        matched += rows < 6 && mt_int(mt_bound(row, "x")) == FACTORS[rows][0] &&
                   mt_int(mt_bound(row, "y")) == FACTORS[rows][1];
        rows++;
    }
    assert(rows == 6 && matched == 6 && "every factor pair of 25, in order, read by name");

    assert(answers_are(mt_eval(m, WHERE(E("#>=", V("x"), 0), E("let", 25, E("square", V("x")), V("x")))), E(5))
           && "a bound posted first decides the root");
    assert(list_is(mt_all(mt_solve(m, N(20), composed())), E(4)) && "the # family solves the composed query");

    for (size_t i = 0; i < sizeof ARITH / sizeof *ARITH; i++)
        assert(answers_are(mt_eval(m, E(ARITH[i].op, ARITH[i].a, ARITH[i].b)), E(ARITH[i].c(ARITH[i].a, ARITH[i].b))) && ARITH[i].op);
    for (size_t i = 0; i < sizeof COMPARE / sizeof *COMPARE; i++)
        assert(answers_are(mt_eval(m, E(COMPARE[i].op, COMPARE[i].a, COMPARE[i].b)), E(B(COMPARE[i].c)))
               && COMPARE[i].op);

    int64_t a = 0;
    mt_rows (row, mt_solve(m, N(20), composed())) a = mt_int(mt_bound(row, "a"));
    assert(a == 4 && "and read by name, through two constraints");
    mt_close(m);
    return 0;
}
