/* Purpose: forward-mode derivatives as an algebra. A dual number
 *   (Dual value derivative) is added and multiplied by two C functions, the
 *   algebra dual names them in the catalog, and match-under carries a value
 *   and its derivative through two tagged rules without the rules knowing.
 * Guarantees: d(3x)/dx at x = 2 comes out as (Dual 6.0 3.0)
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
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
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

    assert(answers_are(mt_eval(m, E("match-under", "&self", "dual", E("output", "a"))), E(E(E("output", "a"), E("Dual", 6.0, 3.0))))
           && "the derivative rides along with the value");
    require("withdraw dual-add", mt_undef(m, "dual-add"));
    require("withdraw dual-multiply", mt_undef(m, "dual-multiply"));
    mt_close(m);
    return 0;
}
