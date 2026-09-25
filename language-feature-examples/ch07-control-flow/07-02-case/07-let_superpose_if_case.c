/* Purpose: four forms at once. progme fans out 2..5, filters with if and
 *   dispatches the rest with case; f is a C function ignoring its argument.
 *   C computes the answers it expects with the same control flow, a loop,
 *   an if and a switch.
 * Guarantees: the original's claim holds [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

static mt_status f(mt_call *call, void *user)
{
    (void)call;
    (void)user;
    return mt_answer(call, N(42));
}

int main(void)
{
    metta *m = open_engine();
    require("publish f", mt_def(m, (mt_op){ .name = "f", .arity = 1, .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = f }));
    require("progme", mt_add(m, E("=", E("progme"),
                                 E("let", V("y"), E("superpose", E(2, 3, 4, 5)),
                                   E("if", E(">", V("y"), 2),
                                     E("case", E(1, V("y")), E(E(E(1, 3), E("f", 0)), E(E(1, 4), E(42, 42)), E(V("else"), E(42, 42, 42)))),
                                     "answertoeverything")))));

    mt_atom *want[4];
    size_t n = 0;
    for (int64_t y = 2; y <= 5; y++) {
        if (y > 2) {
            switch (y) {
            case 3: want[n++] = N(42); break;          /* (f 0) */
            case 4: want[n++] = E(42, 42); break;
            default: want[n++] = E(42, 42, 42); break;
            }
        } else {
            want[n++] = S("answertoeverything");
        }
    }
    check_list_("(progme)", mt_all(mt_eval(m, E("progme"))), n, want);
    return done(m);
}
