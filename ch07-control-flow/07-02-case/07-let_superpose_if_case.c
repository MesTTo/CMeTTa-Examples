/* Purpose: four forms at once. progme fans out 2..5, filters with if and
 *   dispatches the rest with case; f is a C function ignoring its argument.
 *   C computes the answers it expects with the same control flow, a loop,
 *   an if and a switch.
 * Guarantees: the original's claim holds [tested 2026-09-27T00:35:58+10:00:
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

static mt_status f(mt_call *call, void *user)
{
    (void)call;
    (void)user;
    return mt_answer(call, N(42));
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
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
    assert(list_is(mt_all(mt_eval(m, E("progme"))), mt_exprv(n, want)) && "(progme)");
    mt_close(m);
    return 0;
}
