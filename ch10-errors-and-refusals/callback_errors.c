/* Purpose: a C function refuses with words. mt_fail() turns a bad argument
 *   into an engine error whose message reaches the caller as a status, and
 *   the next request after a refusal is served normally.
 * Guarantees: the refusal's reason crosses back, and the function still
 *   answers afterwards [tested 2026-09-27T00:35:58+10:00:
 *   make -C extensions/cmetta corpus-check].
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

static mt_status positive(mt_call *call, void *user)
{
    (void)user;
    mt_clear();
    int64_t value = mt_int(mt_arg(call, 0));
    if (!mt_ok()) return mt_fail(call, "positive expects an integer");
    if (value < 0) return mt_fail(call, "positive refuses negative input");
    return mt_answer(call, N(value));
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("publish positive", mt_def(m, (mt_op){ .name = "positive", .arity = 1,
                                                   .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = positive }));
    mt_clear();
    mt_atom *refused = mt_first(mt_eval(m, E("positive", -1)));
    assert(refused == NULL && mt_error() == MT_ERROR && "the refusal is an error status");
    assert(mt_errmsg() && strstr(mt_errmsg(), "negative input") != NULL
           && "carrying the function's own words");
    mt_clear();
    assert(mt_one_int(mt_eval(m, E("positive", 42))) == 42 && "the next request is served");
    require("withdraw positive", mt_undef(m, "positive"));
    mt_close(m);
    return 0;
}
