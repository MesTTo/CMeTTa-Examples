/* Purpose: carry a value and derivative through two tagged rules into C.
 * Owns resources: callback arguments are borrowed; results transfer to the engine.
 * Guarantees: differentiating 3*x at x=2 gives value 6 and derivative 3
 *   [tested: make check; commit=6022c3f48b6dc64752c6e49cfe9d985c7ac7a4e9].
 * Open Obligations: tensor-runtime interoperability is outside this C example.
 */
#include "common.h"
typedef struct dual { double value, derivative; } dual;

static bool decode(const mt_atom *atom, dual *number)
{
    if (mt_len(atom) != 3) return false;
    const char *head = mt_name(mt_at(atom, 0));
    if (!head || strcmp(head, "Dual") != 0) return false;
    number->value = mt_float(mt_at(atom, 1));
    number->derivative = mt_float(mt_at(atom, 2));
    return mt_ok();
}

static mt_status combine(mt_call *call, void *user)
{
    dual a, b;
    if (!decode(mt_arg(call, 0), &a) || !decode(mt_arg(call, 1), &b))
        return mt_fail(call, "dual arithmetic expects (Dual value derivative)");
    bool product = user != NULL;
    return mt_answer(call, mt_expr("Dual",
        product ? a.value * b.value : a.value + b.value,
        product ? a.derivative * b.value + a.value * b.derivative
                : a.derivative + b.derivative));
}

int main(void)
{
    metta *m = open_engine();
    bool multiplication = true;
    check("publish dual sum", mt_def(m, (mt_op){.name="dual-add", .arity=2,
        .effect=MT_PURE, .fn=combine}));
    check("publish dual product", mt_def(m, (mt_op){.name="dual-multiply", .arity=2,
        .effect=MT_PURE, .fn=combine, .user=&multiplication}));
    check("declare dual carrier and program", mt_do(m,
        "!(add-atom &metta (algebra dual dual-add dual-multiply (Dual 0.0 0.0) "
        "(Dual 1.0 0.0) (laws) (carrier) (requires) global)) "
        "(fact (Dual 2.0 1.0) (source a)) (fact (Dual 3.0 0.0) (scale a)) "
        "(rule (Dual 1.0 0.0) (middle $x) (premises (source $x))) "
        "(rule (Dual 1.0 0.0) (output $x) (premises (middle $x) (scale $x)))"));
    check_answers("two-rule derivative", mt_run(m, "!(match-under &self dual (output a))"),
        "((output a) (Dual 6.0 3.0))");
    check("withdraw sum", mt_undef(m, "dual-add"));
    check("withdraw product", mt_undef(m, "dual-multiply"));
    return done(m, "gradient_algebra");
}
