/* Purpose: a published name is exactly the name written. word_count and
 *   word-count are two C functions, and neither is spelled into the other.
 * Guarantees: each name answers its own function
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

static mt_status constant(mt_call *call, void *user)
{
    return mt_answer(call, N(*(const int *)user));
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    static const int underscore = 7, hyphen = 9;
    require("publish word_count", mt_def(m, (mt_op){ .name = "word_count", .arity = 0,
        .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = constant, .user = (void *)&underscore }));
    require("publish word-count", mt_def(m, (mt_op){ .name = "word-count", .arity = 0,
        .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = constant, .user = (void *)&hyphen }));
    assert(mt_one_int(mt_eval(m, E("word_count"))) == 7 && "word_count is its own function");
    assert(mt_one_int(mt_eval(m, E("word-count"))) == 9 && "word-count is another");
    require("withdraw word_count", mt_undef(m, "word_count"));
    require("withdraw word-count", mt_undef(m, "word-count"));
    mt_close(m);
    return 0;
}
