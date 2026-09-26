/* Purpose: a symbol with two arrows. C keeps the declarations in a table
 *   and types each question from it: a symbol answers its declared types in
 *   order; an application of a function answers each arrow's result whose
 *   parameter is the argument's type, so blacksmith applied to iron makes a
 *   Sword and a Paperclip; and an expression whose head is no function
 *   answers the expression of its parts' types, one per combination.
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

typedef struct declaration {
    const char *term;
    mt_atom *type;
} declaration;

/* Every declared type of a symbol, in order. */
static mt_atom *types_of(const declaration *table, size_t n, const char *term)
{
    mt_atom **found = malloc((n + 1) * sizeof *found);
    size_t k = 0;
    require("room for the types", found != NULL);
    for (size_t i = 0; i < n; i++)
        if (strcmp(table[i].term, term) == 0) found[k++] = mt_keep(table[i].type);
    mt_atom *out = mt_exprv(k, found);
    free(found);
    return out;
}

/* (f x)'s types: each arrow of f whose one parameter is one of x's types
   answers its result; when f has no arrow, the expression of each pair of
   f's and x's types. */
static mt_atom *application(const declaration *table, size_t n, const char *f, const char *x)
{
    mt_atom *fs = types_of(table, n, f), *xs = types_of(table, n, x), **out = malloc((mt_len(fs) * mt_len(xs) + 1) * sizeof *out);
    size_t k = 0;
    bool function = false;
    require("room for the types", out != NULL);
    for (size_t i = 0; i < mt_len(fs); i++) {
        const mt_atom *t = mt_at(fs, i);
        bool arrow = mt_kind_of(t) == MT_EXPR && mt_len(t) == 3 && mt_kind_of(mt_at(t, 0)) == MT_SYMBOL && strcmp(mt_name(mt_at(t, 0)), "->") == 0;
        function |= arrow;
        for (size_t j = 0; arrow && j < mt_len(xs); j++)
            if (mt_eq(mt_at(t, 1), mt_at(xs, j))) out[k++] = mt_keep(mt_at(t, 2));
    }
    for (size_t i = 0; !function && i < mt_len(fs); i++)
        for (size_t j = 0; j < mt_len(xs); j++) out[k++] = E(mt_keep(mt_at(fs, i)), mt_keep(mt_at(xs, j)));
    mt_drop(fs), mt_drop(xs);
    mt_atom *types = mt_exprv(k, out);
    free(out);
    return types;
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    declaration table[] = {
        { "blacksmith", E("->", "Metal", "Sword") }, { "blacksmith", E("->", "Metal", "Paperclip") }, { "iron", S("Metal") }, { "gold", S("Metal") },
    };
    size_t n = sizeof table / sizeof *table;
    for (size_t i = 0; i < n; i++) require("declare a type", mt_add(m, E(":", table[i].term, mt_keep(table[i].type))));

    mt_atom *iron = types_of(table, n, "iron");
    assert(answers_are(mt_eval(m, E("get-type", "iron")), E(mt_keep(mt_at(iron, 0)))) && "iron's type");
    mt_drop(iron);
    assert(answers_are(mt_eval(m, E("collapse", E("get-type", "blacksmith"))), E(types_of(table, n, "blacksmith"))) && "both of blacksmith's arrows");
    assert(answers_are(mt_eval(m, E("collapse", E("get-type", E("blacksmith", "iron")))), E(application(table, n, "blacksmith", "iron")))
           && "an application answers each result");
    assert(answers_are(mt_eval(m, E("collapse", E("get-type", E("iron", "blacksmith")))), E(application(table, n, "iron", "blacksmith")))
           && "data answers its parts' types");
    for (size_t i = 0; i < n; i++) mt_drop(table[i].type);
    mt_close(m);
    return 0;
}
