/* Purpose: the engine's numbers read against C's. == compares terms, so the
 *   integer 1 and the float 1.0 differ where C's own 1 == 1.0 holds; division
 *   by zero is an Error answer where C would trap; each -math operation is a
 *   row of a table beside the <math.h> function it names, the rounding ones
 *   answering as integers what libm answers as doubles, and pow keeping its
 *   operands' kinds; INFINITY and NAN cross as the floats they are; and the
 *   least and greatest of an expression are a C loop comparing its children
 *   with mt_compare. The string the -math guard refuses comes from a C
 *   function.
 * Guarantees: all thirty-four claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 * Build: cc 01-math.c $(pkg-config --cflags --libs cmetta) -lm
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

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

/* Whether an atom is want up to renaming variables; takes both, and shows
   them when they differ. */
static inline bool atom_is(mt_atom *got, mt_atom *want)
{
    bool holds = got && want && mt_alpha_eq(got, want);
    if (!holds) fprintf(stderr, "  got %s\n  want %s\n", got ? mt_show(got) : "nothing",
                        want ? mt_show(want) : "nothing");
    mt_drop(got);
    mt_drop(want);
    return holds;
}

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
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;

    assert(answers_are(mt_eval(m, E("==", 1, 1.0)), E(B(false))) && "(== 1 1.0) compares terms");
    assert(answers_are(mt_eval(m, E("!=", 1.0, 1)), E(B(true))) && "(!= 1.0 1)");
    assert(answers_are(mt_eval(m, E("==", 1, T("s"))), E(B(false))) && "a number is not text");
    assert(answers_are(mt_eval(m, E("==", "true", 1)), E(B(false))) && "nor the symbol true");

    assert(answers_are(mt_eval(m, E("/", 7, 0)), E(E("Error", E("/", 7, 0), "DivisionByZero")))
           && "(/ 7 0) is an answer");
    assert(answers_are(mt_eval(m, E("%", 7, 0)), E(E("Error", E("%", 7, 0), "DivisionByZero")))
           && "(% 7 0) too");
    assert(list_is(mt_all(mt_eval(m, E("/", 7, 0))), E(E("Error", E("/", 7, 0), "DivisionByZero")))
           && "and there is exactly one of it");

    require("publish math-string", mt_def(m, (mt_op){ .name = "math-string", .arity = 0,
                                                    .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = math_string }));
    assert(answers_are(mt_eval(m, E("sqrt-math", E("math-string"))), E(E("Error", E("sqrt-math", T("s")), E("BadArgType", 1, "Number", "String"))))
           && "a computed string meets sqrt-math's own guard");

    /* pow keeps its operands' kinds: the integer 8, where C's pow is 8.0. */
    assert(answers_are(mt_eval(m, E("pow-math", 2, 3)), E(8)) && "(pow-math 2 3)");
    assert(answers_are(mt_eval(m, E("isnan-math", E("sqrt-math", -1))), E(B(true))) && "the root of -1 is NaN");
    assert(answers_are(mt_eval(m, E("pow-math", 0, -1)), E(E("Error", E("pow-math", 0, -1), "DivisionByZero")))
           && "integer 0 to -1 divides by zero");
    assert(answers_are(mt_eval(m, E("isinf-math", E("pow-math", 0.0, -1.0))), E(B(true)))
           && "float 0.0 to -1.0 is infinite");
    assert(answers_are(mt_eval(m, E("pow-math", 2, INT64_C(2147483648))), E(E("Error", E("pow-math", 2, INT64_C(2147483648)),
                                                                              T("power argument is too big, try using float value"))))
           && "an integer exponent is bounded to int32");
    assert(answers_are(mt_eval(m, E("pow-math", 1, 2147483648.0)), E(1)) && "1 to any power is the integer 1");

    for (size_t i = 0; i < sizeof REAL / sizeof *REAL; i++)
        assert(answers_are(mt_eval(m, E(REAL[i].op, REAL[i].arg)), E(REAL[i].libm((double)REAL[i].arg)))
               && REAL[i].op);
    assert(answers_are(mt_eval(m, E("abs-math", -5)), E((int64_t)llabs(-5))) && "abs-math keeps an integer");
    assert(answers_are(mt_eval(m, E("log-math", 10, 100)), E(log(100.0) / log(10.0))) && "(log-math 10 100)");
    for (size_t i = 0; i < sizeof ROUNDING / sizeof *ROUNDING; i++)
        assert(answers_are(mt_eval(m, E(ROUNDING[i].op, ROUNDING[i].arg)), E((int64_t)ROUNDING[i].libm(ROUNDING[i].arg)))
               && ROUNDING[i].op);
    for (size_t i = 0; i < sizeof CLASSIFY / sizeof *CLASSIFY; i++)
        assert(answers_are(mt_eval(m, E(CLASSIFY[i].op, CLASSIFY[i].arg)), E(B(CLASSIFY[i].c(CLASSIFY[i].arg))))
               && CLASSIFY[i].op);

    /* min-atom and max-atom: an expression's children are an array. */
    mt_atom *numbers = E(2, 6, 7, 4, 9, 3);
    const mt_atom *least = mt_at(numbers, 0), *greatest = least;
    for (size_t i = 1; i < mt_len(numbers); i++) {
        if (mt_compare(mt_at(numbers, i), least) < 0) least = mt_at(numbers, i);
        if (mt_compare(mt_at(numbers, i), greatest) > 0) greatest = mt_at(numbers, i);
    }
    assert(atom_is(mt_keep(least), N(2)) && "the least of (2 6 7 4 9 3)");
    assert(atom_is(mt_keep(greatest), N(9)) && "the greatest");
    mt_drop(numbers);
    mt_close(m);
    return 0;
}
