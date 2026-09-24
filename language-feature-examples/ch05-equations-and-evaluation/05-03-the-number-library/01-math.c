/* Purpose: the engine's numbers read against C's. == compares terms, so the
 *   integer 1 and the float 1.0 differ where C's own 1 == 1.0 holds; division
 *   by zero is an Error answer where C would trap; each -math operation is a
 *   row of a table beside the <math.h> function it names, the rounding ones
 *   answering as integers what libm answers as doubles, and pow keeping its
 *   operands' kinds; INFINITY and NAN cross as the floats they are; and the
 *   least and greatest of an expression are a C loop comparing its children
 *   with mt_compare. The string the -math guard refuses comes from a C
 *   function.
 * Guarantees: all thirty-four claims of the original hold [tested: make
 *   twins; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include <math.h>

/* Operations answering the float libm computes, from an integer they promote. */
static const struct { const char *op; int64_t arg; double (*libm)(double); } REAL[] = {
    { "sqrt-math", 9, sqrt }, { "sin-math", 0, sin }, { "asin-math", 0, asin },
    { "cos-math", 0, cos },   { "acos-math", 1, acos }, { "tan-math", 0, tan },
    { "atan-math", 0, atan },
};

/* Operations answering, as an integer, what libm answers as a double. */
static const struct { const char *op; double arg; double (*libm)(double); } ROUNDING[] = {
    { "trunc-math", 5.6, trunc }, { "ceil-math", 5.2, ceil }, { "floor-math", 5.8, floor },
    { "round-math", 5.4, round }, { "round-math", 5.6, round },
};

/* The classifications, which C spells as macros and so wraps once each. */
static bool c_isnan(double x) { return isnan(x); }
static bool c_isinf(double x) { return isinf(x); }
static const struct { const char *op; double arg; bool (*c)(double); } CLASSIFY[] = {
    { "isnan-math", 0.0, c_isnan },  { "isinf-math", 0.0, c_isinf },
    { "isinf-math", INFINITY, c_isinf }, { "isnan-math", NAN, c_isnan },
};

static mt_status math_string(mt_call *call, void *user)
{
    (void)user;
    return mt_answer(call, T("s"));
}

int main(void)
{
    metta *m = open_engine();

    check_answers("(== 1 1.0) compares terms", mt_eval(m, E("==", 1, 1.0)), B(false));
    check_answers("(!= 1.0 1)", mt_eval(m, E("!=", 1.0, 1)), B(true));
    check_answers("a number is not text", mt_eval(m, E("==", 1, T("s"))), B(false));
    check_answers("nor the symbol true", mt_eval(m, E("==", "true", 1)), B(false));

    check_answers("(/ 7 0) is an answer", mt_eval(m, E("/", 7, 0)),
                  E("Error", E("/", 7, 0), "DivisionByZero"));
    check_answers("(% 7 0) too", mt_eval(m, E("%", 7, 0)),
                  E("Error", E("%", 7, 0), "DivisionByZero"));
    check_list("and there is exactly one of it", mt_all(mt_eval(m, E("/", 7, 0))),
               E("Error", E("/", 7, 0), "DivisionByZero"));

    require("publish math-string", mt_def(m, (mt_op){ .name = "math-string", .arity = 0,
                                                    .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = math_string }));
    check_answers("a computed string meets sqrt-math's own guard",
                  mt_eval(m, E("sqrt-math", E("math-string"))),
                  E("Error", E("sqrt-math", T("s")), E("BadArgType", 1, "Number", "String")));

    /* pow keeps its operands' kinds: the integer 8, where C's pow is 8.0. */
    check_answers("(pow-math 2 3)", mt_eval(m, E("pow-math", 2, 3)), 8);
    check_answers("the root of -1 is NaN", mt_eval(m, E("isnan-math", E("sqrt-math", -1))), B(true));
    check_answers("integer 0 to -1 divides by zero", mt_eval(m, E("pow-math", 0, -1)),
                  E("Error", E("pow-math", 0, -1), "DivisionByZero"));
    check_answers("float 0.0 to -1.0 is infinite",
                  mt_eval(m, E("isinf-math", E("pow-math", 0.0, -1.0))), B(true));
    check_answers("an integer exponent is bounded to int32",
                  mt_eval(m, E("pow-math", 2, INT64_C(2147483648))),
                  E("Error", E("pow-math", 2, INT64_C(2147483648)),
                    T("power argument is too big, try using float value")));
    check_answers("1 to any power is the integer 1", mt_eval(m, E("pow-math", 1, 2147483648.0)), 1);

    for (size_t i = 0; i < sizeof REAL / sizeof *REAL; i++)
        check_answers(REAL[i].op, mt_eval(m, E(REAL[i].op, REAL[i].arg)),
                      REAL[i].libm((double)REAL[i].arg));
    check_answers("abs-math keeps an integer", mt_eval(m, E("abs-math", -5)), (int64_t)llabs(-5));
    check_answers("(log-math 10 100)", mt_eval(m, E("log-math", 10, 100)), log(100.0) / log(10.0));
    for (size_t i = 0; i < sizeof ROUNDING / sizeof *ROUNDING; i++)
        check_answers(ROUNDING[i].op, mt_eval(m, E(ROUNDING[i].op, ROUNDING[i].arg)),
                      (int64_t)ROUNDING[i].libm(ROUNDING[i].arg));
    for (size_t i = 0; i < sizeof CLASSIFY / sizeof *CLASSIFY; i++)
        check_answers(CLASSIFY[i].op, mt_eval(m, E(CLASSIFY[i].op, CLASSIFY[i].arg)),
                      B(CLASSIFY[i].c(CLASSIFY[i].arg)));

    /* min-atom and max-atom: an expression's children are an array. */
    mt_atom *numbers = E(2, 6, 7, 4, 9, 3);
    const mt_atom *least = mt_at(numbers, 0), *greatest = least;
    for (size_t i = 1; i < mt_len(numbers); i++) {
        if (mt_compare(mt_at(numbers, i), least) < 0) least = mt_at(numbers, i);
        if (mt_compare(mt_at(numbers, i), greatest) > 0) greatest = mt_at(numbers, i);
    }
    check_atom("the least of (2 6 7 4 9 3)", mt_keep(least), N(2));
    check_atom("the greatest", mt_keep(greatest), N(9));
    mt_drop(numbers);
    return done(m);
}
