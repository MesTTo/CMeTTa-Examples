/* Purpose: time control and interpreter pragmas. timeout, elapsed and
 *   inferences take their expression unevaluated, so C builds it as a term;
 *   a bound that is not reached changes nothing, so each answer is what C
 *   computes for the bounded expression itself: spin's done, a sum, every
 *   answer of a superpose. A pragma answers unit; a stack depth has to be a
 *   nonnegative integer, which C checks to build the error the engine
 *   answers for -1; under a depth of 20 a factorial still answers C's 120
 *   beside the branch that ran out. Relational arithmetic runs backwards,
 *   and C solves each equation by the inverse operation, with no integer
 *   doubling to 7.
 * Guarantees: all twenty-five claims of the original hold, with its five
 *   unasserted forms checked as well [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

static int64_t factorial(int64_t n) { return n ? n * factorial(n - 1) : 1; }

/* $x in (op $x k) == y, solved exactly: NULL when no integer does. */
static mt_atom *solved(char op, int64_t k, int64_t y)
{
    switch (op) {
    case '-': return N(y + k);
    case '+': return N(y - k);
    case '*': return y % k == 0 ? N(y / k) : NULL;
    default: return N(y * k);
    }
}

int main(void)
{
    metta *m = open_engine();
    require("(= (spin $n) ...)", mt_add(m, E("=", E("spin", V("n")), E("if", E(">", V("n"), 0), E("spin", E("-", V("n"), 1)), "done"))));
    mt_atom *finished = S("done"), *three = E(1, 2, 3);
    check_answers("a bound not reached", mt_eval(m, E("timeout", 30, E("spin", 100))), mt_keep(finished));
    check_answers("on a sum", mt_eval(m, E("timeout", 30, E("+", 1, 2))), N(1 + 2));
    check_answers("keeps every answer", mt_eval(m, E("collapse", E("timeout", 30, E("superpose", mt_keep(three))))), mt_keep(three));
    check_answers("elapsed pairs the value with its cost", mt_eval(m, E("let", V("timed"), E("elapsed", E("spin", 100)), E("car-atom", V("timed")))), mt_keep(finished));
    check_answers("sleep answers True", mt_eval(m, E("sleep", 0.01)), B(true));
    check_answers("metta interprets in a space", mt_eval(m, E("metta", E("+", 1, 2), "%Undefined%", "&self")), N(1 + 2));
    check_answers("and so does evalc", mt_eval(m, E("evalc", E("+", 1, 2), "&self")), N(1 + 2));

    const struct { const char *key; mt_atom *value; } settings[] = {
        { "max-time", N(30) }, { "max-inferences", N(100000000) }, { "max-time", S("none") }, { "max-inferences", S("none") }, { "max-stack-depth", N(0) },
    };
    for (size_t i = 0; i < sizeof settings / sizeof *settings; i++) check_answers(settings[i].key, mt_eval(m, E("pragma!", settings[i].key, settings[i].value)), mt_unit());
    const int64_t depth = -1;
    mt_atom *refused = E("pragma!", "max-stack-depth", depth);
    check_answers("a depth below zero", mt_eval(m, mt_keep(refused)), depth >= 0 ? mt_unit() : E("Error", mt_keep(refused), "UnsignedIntegerIsExpected"));
    mt_drop(refused);
    check_answers("the depth cleared", mt_eval(m, E("pragma!", "max-stack-depth", "none")), mt_unit());

    check_answers("a depth of 20", mt_eval(m, E("pragma!", "max-stack-depth", 20)), mt_unit());
    require("(= (bounded-factorial 0) 1)", mt_add(m, E("=", E("bounded-factorial", 0), 1)));
    require("(= (bounded-factorial $n) ...)", mt_add(m, E("=", E("bounded-factorial", V("n")), E("*", V("n"), E("bounded-factorial", E("-", V("n"), 1))))));
    check_answers("a finite branch survives an exhausted one", mt_eval(m, E("bounded-factorial", 5)), N(factorial(5)), E("Error", -3, "StackOverflow"));
    check_answers("the depth cleared again", mt_eval(m, E("pragma!", "max-stack-depth", "none")), mt_unit());

    check_answers("an inference bound not reached", mt_eval(m, E("inferences", 100000, E("spin", 100))), mt_keep(finished));
    check_answers("keeps every answer", mt_eval(m, E("collapse", E("inferences", 100000, E("superpose", mt_keep(three))))), mt_keep(three));
    check_answers("a scoped setting", mt_eval(m, E("with-pragma!", E(E("max-inferences", 100000)), E("+", 20, 22))), N(20 + 22));
    check_answers("two of them", mt_eval(m, E("with-pragma!", E(E("max-time", 30), E("max-inferences", 100000)), E("spin", 100))), mt_keep(finished));
    check_answers("and none afterwards", mt_eval(m, E("spin", 2000)), mt_keep(finished));

    check_answers("verification on", mt_eval(m, E("pragma!", "verify-specializations", B(true))), mt_unit());
    require("verified-inc", mt_add(m, E("=", E("verified-inc", V("x")), E("+", V("x"), 1))));
    require("verified-twice", mt_add(m, E("=", E("verified-twice", V("f"), V("x")), E(V("f"), E(V("f"), V("x"))))));
    check_answers("a verified specialization", mt_eval(m, E("verified-twice", "verified-inc", 20)), N(20 + 1 + 1));
    check_answers("verification off", mt_eval(m, E("pragma!", "verify-specializations", B(false))), mt_unit());

    const struct { char op; int64_t k, y; } equations[] = { { '-', 1, 4 }, { '+', 3, 10 }, { '*', 2, 6 }, { '/', 2, 3 } };
    for (size_t i = 0; i < sizeof equations / sizeof *equations; i++) {
        char op[2] = { equations[i].op, 0 };
        check_answers("arithmetic backwards", mt_eval(m, E("let", equations[i].y, E(op, V("x"), equations[i].k), V("x"))), solved(equations[i].op, equations[i].k, equations[i].y));
    }
    mt_atom *none = solved('*', 2, 7);
    check_answers("no integer doubles to 7", mt_eval(m, E("collapse", E("let", 7, E("*", V("x"), 2), V("x")))), none ? E(none) : mt_unit());
    mt_drop(finished), mt_drop(three);
    return done(m);
}
