/* Purpose: a published name is exactly the name written. word_count and
 *   word-count are two C functions, and neither is spelled into the other.
 * Guarantees: each name answers its own function [tested: make check;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

static mt_status constant(mt_call *call, void *user)
{
    return mt_answer(call, N(*(const int *)user));
}

int main(void)
{
    metta *m = open_engine();
    static const int underscore = 7, hyphen = 9;
    require("publish word_count", mt_def(m, (mt_op){ .name = "word_count", .arity = 0,
        .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = constant, .user = (void *)&underscore }));
    require("publish word-count", mt_def(m, (mt_op){ .name = "word-count", .arity = 0,
        .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = constant, .user = (void *)&hyphen }));
    check_int("word_count is its own function", mt_one_int(mt_eval(m, E("word_count"))), 7);
    check_int("word-count is another", mt_one_int(mt_eval(m, E("word-count"))), 9);
    require("withdraw word_count", mt_undef(m, "word_count"));
    require("withdraw word-count", mt_undef(m, "word-count"));
    return done(m);
}
