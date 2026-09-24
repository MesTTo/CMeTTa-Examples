/* Purpose: an error is a value, which is how C reports one too: a result is
 *   a value or an (Error ...) the caller inspects. C computes each sum it can
 *   and makes an error of the rest the way the engine does: an operand that
 *   is no number, whether declared a String or never declared, and two
 *   unbound operands, which catch turns into an error, and an integer
 *   division by zero. if-error asks whether a value is an error, and
 *   return-on-error answers an error itself and anything else's next form.
 * Guarantees: all eight claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

static bool is_error(const mt_atom *x)
{
    return mt_kind_of(x) == MT_EXPR && mt_len(x) > 0 && mt_kind_of(mt_at(x, 0)) == MT_SYMBOL && strcmp(mt_name(mt_at(x, 0)), "Error") == 0;
}

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

/* C's arithmetic over two operands: a number when both are integers and the
   operation is defined, an error otherwise. */
static mt_atom *arithmetic(char op, const mt_atom *a, const mt_atom *b)
{
    bool numbers = mt_kind_of(a) == MT_INT && mt_kind_of(b) == MT_INT;
    if (numbers && op == '+') return N(mt_int(a) + mt_int(b));
    if (numbers && op == '/' && mt_int(b) != 0) return N(mt_int(a) / mt_int(b));
    char name[2] = { op, 0 };
    return E("Error", E(name, mt_keep(a), mt_keep(b)), "NoValue");
}

int main(void)
{
    metta *m = open_engine();
    require("import lib_he", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_he")))));
    mt_atom *forty = N(40), *two = N(2), *a = S("a"), *undeclared = S("undeclared-operand"), *left = V("left"), *right = V("right"), *zero = N(0);
    mt_atom *sum = arithmetic('+', forty, two);
    check_answers("a sum that is no error", mt_eval(m, E("let", V("result"), E("catch", E("+", mt_keep(forty), mt_keep(two))), E("if-error", V("result"), "Error", V("result")))),
                  if_error(mt_keep(sum), S("Error"), mt_keep(sum)));
    mt_drop(sum);
    require("(: a String)", mt_add(m, E(":", "a", "String")));
    const struct { const char *claim; char op; mt_atom *x, *y; } refusals[] = {
        { "a String operand", '+', forty, a }, { "an undeclared operand", '+', forty, undeclared }, { "integer division by zero", '/', forty, zero },
    };
    for (size_t i = 0; i < 3; i++) {
        char op[2] = { refusals[i].op, 0 };
        check_answers(refusals[i].claim, mt_eval(m, E("if-error", E(op, mt_keep(refusals[i].x), mt_keep(refusals[i].y)), "Error", "fine")),
                      if_error(arithmetic(refusals[i].op, refusals[i].x, refusals[i].y), S("Error"), S("fine")));
    }
    check_answers("unbound operands", mt_eval(m, E("let", V("result"), E("catch", E("+", mt_keep(left), mt_keep(right))), E("if-error", V("result"), "Error", V("result")))),
                  if_error(arithmetic('+', left, right), S("Error"), S("no error")));
    mt_atom *error = E("Error", 5, "BadType");
    check_answers("an error value", mt_eval(m, E("if-error", mt_keep(error), T("Error!"), T("No error"))), if_error(mt_keep(error), T("Error!"), T("No error")));
    check_answers("return-on-error answers the error", mt_eval(m, E("return-on-error", mt_keep(error), 6)), return_on_error(mt_keep(error), N(6)));
    check_answers("and otherwise the next form", mt_eval(m, E("return-on-error", 5, 6)), return_on_error(N(5), N(6)));
    mt_atom *held[] = { forty, two, a, undeclared, left, right, zero, error };
    for (size_t i = 0; i < sizeof held / sizeof *held; i++) mt_drop(held[i]);
    return done(m);
}
