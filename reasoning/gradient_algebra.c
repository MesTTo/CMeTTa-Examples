/* Purpose: forward-mode derivatives as an algebra. A dual number
 *   (Dual value derivative) is added and multiplied by two C functions, the
 *   algebra dual names them in the catalog, and match-under carries a value
 *   and its derivative through two tagged rules without the rules knowing.
 * Guarantees: d(3x)/dx at x = 2 comes out as (Dual 6.0 3.0) [tested: make
 *   check; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

typedef struct dual { double value, derivative; } dual;

static bool dual_of(const mt_atom *atom, dual *out)
{
    const char *head = mt_name(mt_at(atom, 0));
    if (mt_len(atom) != 3 || !head || strcmp(head, "Dual") != 0) return false;
    mt_clear();
    out->value = mt_float(mt_at(atom, 1));
    out->derivative = mt_float(mt_at(atom, 2));
    return mt_ok();
}

/* One function for both operations: user is non-NULL for the product. */
static mt_status combine(mt_call *call, void *user)
{
    dual a, b;
    if (!dual_of(mt_arg(call, 0), &a) || !dual_of(mt_arg(call, 1), &b))
        return mt_fail(call, "dual arithmetic wants (Dual value derivative)");
    if (user)   /* the product rule */
        return mt_answer(call, E("Dual", a.value * b.value,
                                 a.derivative * b.value + a.value * b.derivative));
    return mt_answer(call, E("Dual", a.value + b.value, a.derivative + b.derivative));
}

int main(void)
{
    metta *m = open_engine();
    static bool product = true;
    require("publish dual-add", mt_def(m, (mt_op){ .name = "dual-add", .arity = 2,
                                                   .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = combine }));
    require("publish dual-multiply", mt_def(m, (mt_op){ .name = "dual-multiply", .arity = 2,
        .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = combine, .user = &product }));
    /* (algebra dual dual-add dual-multiply (Dual 0.0 0.0) (Dual 1.0 0.0) (laws) (carrier) (requires) global) */
    require("declare the algebra", mt_add(mt_catalog(m), E("algebra", "dual", "dual-add", "dual-multiply",
        E("Dual", 0.0, 0.0), E("Dual", 1.0, 0.0), E("laws"), E("carrier"), E("requires"), "global")));

    /* x = 2 with dx = 1 is the source; the scale 3 is a constant. */
    require("x", mt_add(m, E("fact", E("Dual", 2.0, 1.0), E("source", "a"))));
    require("the scale", mt_add(m, E("fact", E("Dual", 3.0, 0.0), E("scale", "a"))));
    require("a middle rule", mt_add(m, E("rule", E("Dual", 1.0, 0.0), E("middle", V("x")),
                                         E("premises", E("source", V("x"))))));
    require("the output rule", mt_add(m, E("rule", E("Dual", 1.0, 0.0), E("output", V("x")),
                                           E("premises", E("middle", V("x")), E("scale", V("x"))))));

    check_answers("the derivative rides along with the value",
                  mt_eval(m, E("match-under", "&self", "dual", E("output", "a"))),
                  E(E("output", "a"), E("Dual", 6.0, 3.0)));
    require("withdraw dual-add", mt_undef(m, "dual-add"));
    require("withdraw dual-multiply", mt_undef(m, "dual-multiply"));
    return done(m);
}
