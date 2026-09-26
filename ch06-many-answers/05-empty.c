/* Purpose: a function that answers nothing. In C that is a function
 *   returning MT_FAIL: no answer for these arguments, which the engine reads
 *   as (empty), so the call's answer list is empty.
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

static mt_status nothing(mt_call *call, void *user)
{
    (void)call;
    (void)user;
    return MT_FAIL;
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("publish y", mt_def(m, (mt_op){ .name = "y", .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = nothing }));
    assert(!mt_first(mt_eval(m, E("y"))) && mt_ok() && "(y) answers nothing");
    mt_close(m);
    return 0;
}
