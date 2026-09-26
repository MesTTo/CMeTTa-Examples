/* Purpose: throw makes an error value and trace! prints without changing a
 *   value. C's throw answers (Error (throw reason) reason), or the reason
 *   itself when it is an error already, so rethrowing keeps the cause; the
 *   error travels as any answer does, through if-error and return-on-error
 *   and out of half, whose guard C runs as its own function. trace! answers
 *   its second argument, which C computes, while the engine prints the
 *   first.
 * Guarantees: all fifteen claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static bool is_error(const mt_atom *x)
{
    return mt_kind_of(x) == MT_EXPR && mt_len(x) > 0 && mt_kind_of(mt_at(x, 0)) == MT_SYMBOL && strcmp(mt_name(mt_at(x, 0)), "Error") == 0;
}

/* (throw reason): an error value, the reason itself when it is one. */
static mt_atom *thrown(mt_atom *reason) { return is_error(reason) ? reason : E("Error", E("throw", mt_keep(reason)), reason); }

static mt_atom *if_error(mt_atom *value, mt_atom *then, mt_atom *otherwise)
{
    bool error = is_error(value);
    mt_drop(value), mt_drop(error ? otherwise : then);
    return error ? then : otherwise;
}

static mt_atom *return_on_error(mt_atom *value, mt_atom *next)
{
    if (!is_error(value)) return mt_drop(value), next;
    return mt_drop(next), value;
}

/* half's guard: an even n halves, an odd one throws (odd n). */
static mt_atom *half(int64_t n) { return n % 2 == 0 ? N(n / 2) : thrown(E("odd", n)); }

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    mt_atom *reasons[] = { E("my-ball", 1), T("text") };
    for (size_t i = 0; i < 2; i++) assert(answers_are(mt_eval(m, E("throw", mt_keep(reasons[i]))), E(thrown(reasons[i]))) && (i ? "a text reason" : "an expression reason"));
    assert(answers_are(mt_eval(m, E("if-error", E("throw", "oops"), "caught", "fine")), E(if_error(thrown(S("oops")), S("caught"), S("fine")))) && "if-error reads a thrown error");
    assert(answers_are(mt_eval(m, E("if-error", 42, "caught", "fine")), E(if_error(N(42), S("caught"), S("fine")))) && "and a value");
    assert(answers_are(mt_eval(m, E("return-on-error", E("throw", "oops"), "carried-on")), E(return_on_error(thrown(S("oops")), S("carried-on")))) && "return-on-error answers it");
    assert(answers_are(mt_eval(m, E("return-on-error", 42, "carried-on")), E(return_on_error(N(42), S("carried-on")))) && "and passes a value");
    mt_atom *inner = E("Error", E("inner", 1), "because");
    assert(answers_are(mt_eval(m, E("throw", mt_keep(inner))), E(thrown(mt_keep(inner)))) && "an error reason is not wrapped again");
    assert(answers_are(mt_eval(m, E("throw", E("throw", "first"))), E(thrown(thrown(S("first"))))) && "so rethrowing keeps the cause");
    mt_drop(inner);

    require("(= (half $n) ...)", mt_add(m, E("=", E("half", V("n")), E("if", E("==", E("%", V("n"), 2), 0), E("/", V("n"), 2), E("throw", E("odd", V("n")))))));
    assert(answers_are(mt_eval(m, E("half", 10)), E(half(10))) && "an even half");
    assert(answers_are(mt_eval(m, E("half", 7)), E(half(7))) && "an odd one throws");
    assert(answers_are(mt_eval(m, E("if-error", E("half", 7), "refused", E("half", 7))), E(if_error(half(7), S("refused"), half(7)))) && "and if-error sees it");

    assert(answers_are(mt_eval(m, E("trace!", T("the answer"), 42)), E(N(42))) && "trace! answers its value");
    assert(answers_are(mt_eval(m, E("+", 1, E("trace!", T("adding one to"), 41))), E(N(1 + 41))) && "inside an expression");
    assert(answers_are(mt_eval(m, E("trace!", E("half", 10), E("half", 10))), E(half(10))) && "a computed label and value");
    assert(answers_are(mt_eval(m, E("trace!", E("checking", E("odd", 7)), "ok")), E(S("ok"))) && "a label as written");
    mt_close(m);
    return 0;
}
