/* Purpose: a check over every answer. forall holds when its check holds for
 *   every answer of its generator, which in C is an all-of loop that exits
 *   at the first failure. f is a C generator answering each value of an
 *   array, g's equations come from a table, and P is a C predicate; each of
 *   the original's twelve spellings, named functions, lambdas bound to C
 *   variables, lambdas written in place or wrapped in an if, is a table row
 *   whose expected answer C computes with its own loop over the same values.
 * Guarantees: all twelve claims of the original hold
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

static const int64_t F_VALUES[] = { 1, 2 };
static const int64_t G_TABLE[][2] = { { 1, 1 }, { 2, 2 } };

/* An iterator over an array of integers, for mt_answer_iter. */
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

static mt_status below_two(mt_call *call, void *user)
{
    (void)user;
    return mt_answer(call, B(mt_int(mt_arg(call, 0)) < 2));
}

/* C's forall: every value, scaled, is below the limit. */
static bool all_below(const int64_t *values, size_t n, int64_t scale, int64_t limit)
{
    for (size_t i = 0; i < n; i++)
        if (!(values[i] * scale < limit)) return false;
    return true;
}

static mt_atom *below(int64_t limit) { return E("|->", E(V("v")), E("<", V("v"), limit)); }
static mt_atom *wrapped(int64_t limit) { return E("if", B(true), below(limit), 42); }
static mt_atom *genlambda(void) { return E("|->", E(V("x")), E("g", V("x"))); }

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("publish f", mt_def(m, (mt_op){ .name = "f", .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = f }));
    for (size_t i = 0; i < 2; i++)
        require("(= (g k) v)", mt_add(m, E("=", E("g", G_TABLE[i][0]), G_TABLE[i][1])));
    require("publish P", mt_def(m, (mt_op){ .name = "P", .arity = 1, .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = below_two }));

    const int64_t both[] = { 1, 2 }, two[] = { 2 }, one[] = { 1 };
    const struct { const char *claim; mt_atom *generator, *check; const int64_t *values; size_t n;
                   int64_t scale, limit; } rows[] = {
        { "an argument-free generator", E("f"), S("P"), both, 2, 1, 2 },
        { "an argument-ful one", E("g", V("x")), S("P"), both, 2, 1, 2 },
        { "a generator lambda", E(genlambda(), V("z")), S("P"), both, 2, 1, 2 },
        { "a check lambda", E("g", 2), below(2), two, 1, 1, 2 },
        { "that holds", E("g", 1), below(2), one, 1, 1, 2 },
        { "and fails again", E("g", 2), below(2), two, 1, 1, 2 },
        { "two lambdas", E(genlambda(), V("z")), below(2), both, 2, 1, 2 },
        { "written in place", E(genlambda(), V("z")), below(2), both, 2, 1, 2 },
        { "with room to hold", E(genlambda(), V("z")), below(20), both, 2, 1, 20 },
        { "wrapped in an if", E(genlambda(), V("z")), wrapped(2), both, 2, 1, 2 },
        { "wrapped, holding", E(genlambda(), V("z")), wrapped(20), both, 2, 1, 20 },
        { "a scaled generator", E(E("|->", E(V("x")), E("*", 100, E("g", V("x")))), V("z")), wrapped(20), both, 2, 100, 20 },
    };
    for (size_t i = 0; i < sizeof rows / sizeof *rows; i++)
        assert(answers_are(mt_eval(m, E("forall", rows[i].generator, rows[i].check)), E(B(all_below(rows[i].values, rows[i].n, rows[i].scale, rows[i].limit))))
               && rows[i].claim);
    mt_close(m);
    return 0;
}
