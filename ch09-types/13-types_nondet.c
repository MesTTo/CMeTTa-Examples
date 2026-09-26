/* Purpose: one function, two arrows, and a declared result that is checked
 *   like an argument. C tries f's arrows in declaration order: one admits an
 *   argument having its parameter type, runs f's body, a C function here,
 *   and keeps the answer only when the result has the arrow's result type.
 *   So T3in, a Type1 whose body answers the Type2 Tdefault, has no answer
 *   until T3in is declared a Type2 as well.
 * Guarantees: all four claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
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

/* The declarations as C keeps them, in the original's order. */
static const char *table[][2] = {
    { "T1in", "Type1" }, { "T1out", "Type1" }, { "T2in", "Type2" }, { "T2out", "Type2" }, { "T3in", "Type1" }, { "Tdefault", "Type2" }, { "T3in", "Type2" },
};
static const char *arrows[][2] = { { "Type1", "Type1" }, { "Type2", "Type2" } };
#define ARROWS (sizeof arrows / sizeof *arrows)

static bool has_type(const char *term, const char *type, size_t declared)
{
    for (size_t i = 0; i < declared; i++)
        if (strcmp(table[i][0], term) == 0 && strcmp(table[i][1], type) == 0) return true;
    return false;
}

/* f's body: T1in to T1out, T2in to T2out, anything else to Tdefault. */
static const char *body(const char *a) { return strcmp(a, "T1in") == 0 ? "T1out" : strcmp(a, "T2in") == 0 ? "T2out" : "Tdefault"; }

/* f's answers over the first `declared` declarations. */
static mt_atom *answers(const char *a, size_t declared)
{
    mt_atom *out[ARROWS];
    size_t n = 0;
    for (size_t i = 0; i < ARROWS; i++)
        if (has_type(a, arrows[i][0], declared) && has_type(body(a), arrows[i][1], declared)) out[n++] = S(body(a));
    return mt_exprv(n, out);
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    for (size_t i = 0; i < ARROWS; i++) require("declare an arrow", mt_add(m, E(":", "f", E("->", arrows[i][0], arrows[i][1]))));
    require("define f", mt_add(m, E("=", E("f", V("a")), E("if", E("==", V("a"), "T1in"), "T1out", E("if", E("==", V("a"), "T2in"), "T2out", "Tdefault")))));
    for (size_t i = 0; i < 6; i++) require("declare a type", mt_add(m, E(":", table[i][0], table[i][1])));
    const char *inputs[] = { "T1in", "T2in", "T3in" };
    const char *claims[] = { "Type1 to Type1", "Type2 to Type2", "a result the arrow refuses" };
    for (size_t i = 0; i < 3; i++) assert(answers_are(mt_eval(m, E("collapse", E("f", inputs[i]))), E(answers(inputs[i], 6))) && claims[i]);
    require("(: T3in Type2)", mt_add(m, E(":", table[6][0], table[6][1])));
    assert(answers_are(mt_eval(m, E("collapse", E("f", "T3in"))), E(answers("T3in", 7))) && "a second type admits the second arrow");
    mt_close(m);
    return 0;
}
