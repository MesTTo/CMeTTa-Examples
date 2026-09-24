/* Purpose: one fold, ten spellings. foldall folds every answer of a
 *   generator with an aggregator, which in C is a loop over the same values
 *   with a function pointer. f is a C generator, g's equations come from a
 *   table and merge is a C function; each of the original's ten spellings is
 *   a table row whose expected answer is C's fold of the values it
 *   generates, doubled where the generator doubles.
 * Guarantees: all ten claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

static const int64_t VALUES[] = { 2, 3 };
static const int64_t G_TABLE[][2] = { { 1, 2 }, { 2, 3 } };

typedef struct { const int64_t *at, *end; } span;
static mt_status next_value(void *state, mt_atom **answer)
{
    span *s = state;
    if (s->at == s->end) { *answer = NULL; return MT_DONE; }
    *answer = N(*s->at++);
    return MT_ROW;
}
static void free_span(void *state) { free(state); }

/* (f): 2, then 3. */
static mt_status f(mt_call *call, void *user)
{
    (void)user;
    span *s = malloc(sizeof *s);
    if (!s) return mt_fail(call, "no memory for f's answers");
    *s = (span){ VALUES, VALUES + 2 };
    return mt_answer_iter(call, (mt_iterator){ s, next_value, free_span });
}

static int64_t add(int64_t a, int64_t b) { return a + b; }

static mt_status merge(mt_call *call, void *user)
{
    (void)user;
    return mt_answer(call, N(add(mt_int(mt_arg(call, 0)), mt_int(mt_arg(call, 1)))));
}

/* C's foldall: start, then aggregate each value, scaled. */
static int64_t fold(int64_t (*aggregate)(int64_t, int64_t), int64_t scale, int64_t start)
{
    int64_t acc = start;
    for (size_t i = 0; i < sizeof VALUES / sizeof *VALUES; i++) acc = aggregate(acc, scale * VALUES[i]);
    return acc;
}

static mt_atom *adding(void) { return E("|->", E(V("x"), V("y")), E("+", V("x"), V("y"))); }
static mt_atom *chosen(void) { return E("if", B(true), E("let", V("f"), adding(), V("f")), E("empty")); }

int main(void)
{
    metta *m = open_engine();
    require("publish f", mt_def(m, (mt_op){ .name = "f", .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = f }));
    for (size_t i = 0; i < 2; i++)
        require("(= (g k) v)", mt_add(m, E("=", E("g", G_TABLE[i][0]), G_TABLE[i][1])));
    require("publish merge", mt_def(m, (mt_op){ .name = "merge", .arity = 2, .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = merge }));

    const struct { const char *claim; mt_atom *aggregate, *generator; int64_t scale; } rows[] = {
        { "a named aggregator", S("merge"), E("f"), 1 },
        { "over an argument-ful generator", S("merge"), E("g", V("x")), 1 },
        { "a lambda aggregator", adding(), E("f"), 1 },
        { "over g", adding(), E("g", V("z")), 1 },
        { "stated twice", adding(), E("g", V("z")), 1 },
        { "a generator lambda ignoring its argument", adding(), E(E("|->", E(V("z")), E("f")), V("x")), 1 },
        { "one using it", adding(), E(E("|->", E(V("z")), E("g", V("z"))), V("x")), 1 },
        { "both written in place", adding(), E(E("|->", E(V("z")), E("g", V("z"))), V("w")), 1 },
        { "the aggregator out of an if", chosen(), E(E("|->", E(V("z")), E("g", V("z"))), V("w")), 1 },
        { "and a doubling generator", chosen(), E(E("|->", E(V("z")), E("*", 2, E("g", V("z")))), V("w")), 2 },
    };
    for (size_t i = 0; i < sizeof rows / sizeof *rows; i++)
        check_answers(rows[i].claim, mt_eval(m, E("foldall", rows[i].aggregate, rows[i].generator, 0)),
                      fold(add, rows[i].scale, 0));
    return done(m);
}
