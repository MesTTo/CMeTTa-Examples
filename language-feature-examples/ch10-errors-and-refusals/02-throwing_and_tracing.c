/* Purpose: throw makes an error value and trace! prints without changing a
 *   value. C's throw answers (Error (throw reason) reason), or the reason
 *   itself when it is an error already, so rethrowing keeps the cause; the
 *   error travels as any answer does, through if-error and return-on-error
 *   and out of half, whose guard C runs as its own function. trace! answers
 *   its second argument, which C computes, while the engine prints the
 *   first.
 * Guarantees: all fifteen claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

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
    metta *m = open_engine();
    mt_atom *reasons[] = { E("my-ball", 1), T("text") };
    for (size_t i = 0; i < 2; i++) check_answers(i ? "a text reason" : "an expression reason", mt_eval(m, E("throw", mt_keep(reasons[i]))), thrown(reasons[i]));
    check_answers("if-error reads a thrown error", mt_eval(m, E("if-error", E("throw", "oops"), "caught", "fine")), if_error(thrown(S("oops")), S("caught"), S("fine")));
    check_answers("and a value", mt_eval(m, E("if-error", 42, "caught", "fine")), if_error(N(42), S("caught"), S("fine")));
    check_answers("return-on-error answers it", mt_eval(m, E("return-on-error", E("throw", "oops"), "carried-on")), return_on_error(thrown(S("oops")), S("carried-on")));
    check_answers("and passes a value", mt_eval(m, E("return-on-error", 42, "carried-on")), return_on_error(N(42), S("carried-on")));
    mt_atom *inner = E("Error", E("inner", 1), "because");
    check_answers("an error reason is not wrapped again", mt_eval(m, E("throw", mt_keep(inner))), thrown(mt_keep(inner)));
    check_answers("so rethrowing keeps the cause", mt_eval(m, E("throw", E("throw", "first"))), thrown(thrown(S("first"))));
    mt_drop(inner);

    require("(= (half $n) ...)", mt_add(m, E("=", E("half", V("n")), E("if", E("==", E("%", V("n"), 2), 0), E("/", V("n"), 2), E("throw", E("odd", V("n")))))));
    check_answers("an even half", mt_eval(m, E("half", 10)), half(10));
    check_answers("an odd one throws", mt_eval(m, E("half", 7)), half(7));
    check_answers("and if-error sees it", mt_eval(m, E("if-error", E("half", 7), "refused", E("half", 7))), if_error(half(7), S("refused"), half(7)));

    check_answers("trace! answers its value", mt_eval(m, E("trace!", T("the answer"), 42)), N(42));
    check_answers("inside an expression", mt_eval(m, E("+", 1, E("trace!", T("adding one to"), 41))), N(1 + 41));
    check_answers("a computed label and value", mt_eval(m, E("trace!", E("half", 10), E("half", 10))), half(10));
    check_answers("a label as written", mt_eval(m, E("trace!", E("checking", E("odd", 7)), "ok")), S("ok"));
    return done(m);
}
