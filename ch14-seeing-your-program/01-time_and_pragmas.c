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
 *   unasserted forms checked as well [tested 2026-09-27T00:35:58+10:00:
 *   make -C extensions/cmetta corpus-check].
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
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("(= (spin $n) ...)", mt_add(m, E("=", E("spin", V("n")), E("if", E(">", V("n"), 0), E("spin", E("-", V("n"), 1)), "done"))));
    mt_atom *finished = S("done"), *three = E(1, 2, 3);
    assert(answers_are(mt_eval(m, E("timeout", 30, E("spin", 100))), E(mt_keep(finished))) && "a bound not reached");
    assert(answers_are(mt_eval(m, E("timeout", 30, E("+", 1, 2))), E(N(1 + 2))) && "on a sum");
    assert(answers_are(mt_eval(m, E("collapse", E("timeout", 30, E("superpose", mt_keep(three))))), E(mt_keep(three))) && "keeps every answer");
    assert(answers_are(mt_eval(m, E("let", V("timed"), E("elapsed", E("spin", 100)), E("car-atom", V("timed")))), E(mt_keep(finished))) && "elapsed pairs the value with its cost");
    assert(answers_are(mt_eval(m, E("sleep", 0.01)), E(B(true))) && "sleep answers True");
    assert(answers_are(mt_eval(m, E("metta", E("+", 1, 2), "%Undefined%", "&self")), E(N(1 + 2))) && "metta interprets in a space");
    assert(answers_are(mt_eval(m, E("evalc", E("+", 1, 2), "&self")), E(N(1 + 2))) && "and so does evalc");

    const struct { const char *key; mt_atom *value; } settings[] = {
        { "max-time", N(30) }, { "max-inferences", N(100000000) }, { "max-time", S("none") }, { "max-inferences", S("none") }, { "max-stack-depth", N(0) },
    };
    for (size_t i = 0; i < sizeof settings / sizeof *settings; i++) assert(answers_are(mt_eval(m, E("pragma!", settings[i].key, settings[i].value)), E(mt_unit())) && settings[i].key);
    const int64_t depth = -1;
    mt_atom *refused = E("pragma!", "max-stack-depth", depth);
    assert(answers_are(mt_eval(m, mt_keep(refused)), E(depth >= 0 ? mt_unit() : E("Error", mt_keep(refused), "UnsignedIntegerIsExpected"))) && "a depth below zero");
    mt_drop(refused);
    assert(answers_are(mt_eval(m, E("pragma!", "max-stack-depth", "none")), E(mt_unit())) && "the depth cleared");

    assert(answers_are(mt_eval(m, E("pragma!", "max-stack-depth", 20)), E(mt_unit())) && "a depth of 20");
    require("(= (bounded-factorial 0) 1)", mt_add(m, E("=", E("bounded-factorial", 0), 1)));
    require("(= (bounded-factorial $n) ...)", mt_add(m, E("=", E("bounded-factorial", V("n")), E("*", V("n"), E("bounded-factorial", E("-", V("n"), 1))))));
    assert(answers_are(mt_eval(m, E("bounded-factorial", 5)), E(N(factorial(5)), E("Error", -3, "StackOverflow"))) && "a finite branch survives an exhausted one");
    assert(answers_are(mt_eval(m, E("pragma!", "max-stack-depth", "none")), E(mt_unit())) && "the depth cleared again");

    assert(answers_are(mt_eval(m, E("inferences", 100000, E("spin", 100))), E(mt_keep(finished))) && "an inference bound not reached");
    assert(answers_are(mt_eval(m, E("collapse", E("inferences", 100000, E("superpose", mt_keep(three))))), E(mt_keep(three))) && "keeps every answer");
    assert(answers_are(mt_eval(m, E("with-pragma!", E(E("max-inferences", 100000)), E("+", 20, 22))), E(N(20 + 22))) && "a scoped setting");
    assert(answers_are(mt_eval(m, E("with-pragma!", E(E("max-time", 30), E("max-inferences", 100000)), E("spin", 100))), E(mt_keep(finished))) && "two of them");
    assert(answers_are(mt_eval(m, E("spin", 2000)), E(mt_keep(finished))) && "and none afterwards");

    assert(answers_are(mt_eval(m, E("pragma!", "verify-specializations", B(true))), E(mt_unit())) && "verification on");
    require("verified-inc", mt_add(m, E("=", E("verified-inc", V("x")), E("+", V("x"), 1))));
    require("verified-twice", mt_add(m, E("=", E("verified-twice", V("f"), V("x")), E(V("f"), E(V("f"), V("x"))))));
    assert(answers_are(mt_eval(m, E("verified-twice", "verified-inc", 20)), E(N(20 + 1 + 1))) && "a verified specialization");
    assert(answers_are(mt_eval(m, E("pragma!", "verify-specializations", B(false))), E(mt_unit())) && "verification off");

    const struct { char op; int64_t k, y; } equations[] = { { '-', 1, 4 }, { '+', 3, 10 }, { '*', 2, 6 }, { '/', 2, 3 } };
    for (size_t i = 0; i < sizeof equations / sizeof *equations; i++) {
        char op[2] = { equations[i].op, 0 };
        assert(answers_are(mt_eval(m, E("let", equations[i].y, E(op, V("x"), equations[i].k), V("x"))), E(solved(equations[i].op, equations[i].k, equations[i].y))) && "arithmetic backwards");
    }
    mt_atom *none = solved('*', 2, 7);
    assert(answers_are(mt_eval(m, E("collapse", E("let", 7, E("*", V("x"), 2), V("x")))), E(none ? E(none) : mt_unit())) && "no integer doubles to 7");
    mt_drop(finished), mt_drop(three);
    mt_close(m);
    return 0;
}
