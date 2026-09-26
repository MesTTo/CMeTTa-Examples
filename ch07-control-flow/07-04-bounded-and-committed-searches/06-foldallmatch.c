/* Purpose: folding a match and folding a let. The kb facts come from a C
 *   table and f answers a C array; foldall sums (+ n 1) over each, and C
 *   sums the same over its own arrays.
 * Guarantees: both claims of the original hold
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
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    for (size_t i = 0; i < 2; i++) require("(kb n)", mt_add(m, E("kb", KB[i])));
    require("publish f", mt_def(m, (mt_op){ .name = "f", .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = f }));

    assert(answers_are(mt_eval(m, E("foldall", "+", E("match", "&self", E("kb", V("n")), E("+", V("n"), 1)), 0)), E(bumped_sum(KB, 2)))
           && "folding a match");
    assert(answers_are(mt_eval(m, E("foldall", "+", E("let", V("x"), E("f"), E("+", 1, V("x"))), 0)), E(bumped_sum(F_VALUES, 2)))
           && "folding a let");
    mt_close(m);
    return 0;
}
