/* Purpose: a call before its callee exists. (= (f) (g)) is stored while g
 *   means nothing, and it is only a term until something reduces it; g then
 *   arrives as a C function, and both f and h, which call it, answer what C
 *   answers.
 * Guarantees: (f) and (h) are 42 [tested 2026-09-27T00:35:58+10:00:
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

static mt_status g(mt_call *call, void *user)
{
    (void)user;
    return mt_answer(call, N(42));
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("(= (f) (g)), before g exists", mt_add(m, E("=", E("f"), E("g"))));
    require("publish g", mt_def(m, (mt_op){ .name = "g", .arity = 0, .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = g }));
    require("(= (h) (g))", mt_add(m, E("=", E("h"), E("g"))));
    assert(mt_one_int(mt_eval(m, E("f"))) == 42 && "(f) reaches the C function");
    assert(mt_one_int(mt_eval(m, E("h"))) == 42 && "and so does (h)");
    mt_close(m);
    return 0;
}
