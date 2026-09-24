/* Purpose: folding a match and folding a let. The kb facts come from a C
 *   table and f answers a C array; foldall sums (+ n 1) over each, and C
 *   sums the same over its own arrays.
 * Guarantees: both claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

static const int64_t KB[] = { 1, 2 }, F_VALUES[] = { 1, 2 };

typedef struct { const int64_t *at, *end; } span;
static mt_status next_value(void *state, mt_atom **answer)
{
    span *s = state;
    if (s->at == s->end) { *answer = NULL; return MT_DONE; }
    *answer = N(*s->at++);
    return MT_ROW;
}
static void free_span(void *state) { free(state); }

static mt_status f(mt_call *call, void *user)
{
    (void)user;
    span *s = malloc(sizeof *s);
    if (!s) return mt_fail(call, "no memory for f's answers");
    *s = (span){ F_VALUES, F_VALUES + 2 };
    return mt_answer_iter(call, (mt_iterator){ s, next_value, free_span });
}

static int64_t bumped_sum(const int64_t *values, size_t n)
{
    int64_t sum = 0;
    for (size_t i = 0; i < n; i++) sum += values[i] + 1;
    return sum;
}

int main(void)
{
    metta *m = open_engine();
    for (size_t i = 0; i < 2; i++) require("(kb n)", mt_add(m, E("kb", KB[i])));
    require("publish f", mt_def(m, (mt_op){ .name = "f", .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = f }));

    check_answers("folding a match", mt_eval(m, E("foldall", "+", E("match", "&self", E("kb", V("n")), E("+", V("n"), 1)), 0)),
                  bumped_sum(KB, 2));
    check_answers("folding a let", mt_eval(m, E("foldall", "+", E("let", V("x"), E("f"), E("+", 1, V("x"))), 0)),
                  bumped_sum(F_VALUES, 2));
    return done(m);
}
