/* Purpose: a C function the engine calls may call the engine. delegate asks
 *   MeTTa for (double x) while MeTTa is inside delegate, and hands the nested
 *   answer back as its own.
 * Guarantees: (delegate 21) is 42 through C to MeTTa to C to MeTTa
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>

/* A door the program needs before it can go on: on refusal, say why and stop. */
#define require(what, ok) \
    ((ok) ? (void)0 : (fprintf(stderr, "%s: %s\n", (what), mt_errmsg()), exit(EXIT_FAILURE)))

static mt_status delegate(mt_call *call, void *user)
{
    (void)user;
    mt_atom *doubled = mt_one(mt_eval(mt_of(call), E("double", mt_keep(mt_arg(call, 0)))));
    if (!doubled) return mt_error();
    return mt_answer(call, doubled);        /* the nested answer is taken */
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    /* (= (double $x) (* 2 $x)) */
    require("define double", mt_add(m, E("=", E("double", V("x")), E("*", 2, V("x")))));
    require("publish delegate", mt_def(m, (mt_op){ .name = "delegate", .arity = 1,
                                                   .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = delegate }));
    assert(mt_one_int(mt_eval(m, E("delegate", 21))) == 42
           && "C calls MeTTa from inside a call MeTTa made");
    require("withdraw delegate", mt_undef(m, "delegate"));
    mt_close(m);
    return 0;
}
