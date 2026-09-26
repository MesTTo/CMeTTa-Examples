/* Purpose: the engine's own type surface, reported without importing
 *   lib_builtin_types. C keeps that surface as a table, the way a header
 *   lists signatures: a name and its arrows, map-atom with one per spelling.
 *   What C can derive from the table it derives: a parametric arrow applied
 *   to a literal is its result with the parameter's type variable bound to
 *   the literal's type, by mt_unify and mt_substitute; &self holds only the
 *   declaration C added, since the surface is facts and not atoms; and a
 *   program's own declaration of car-atom is answered ahead of the table's.
 * Guarantees: all twenty-five claims of the original hold
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

typedef struct signature {
    const char *name;
    mt_atom *arrow;
} signature;

/* Every arrow the table gives a name, in order. */
static mt_atom *arrows_of(const signature *table, size_t n, const char *name)
{
    mt_atom **found = malloc((n + 1) * sizeof *found);
    size_t k = 0;
    require("room for the arrows", found != NULL);
    for (size_t i = 0; i < n; i++)
        if (strcmp(table[i].name, name) == 0) found[k++] = mt_keep(table[i].arrow);
    mt_atom *out = mt_exprv(k, found);
    free(found);
    return out;
}

/* A one-parameter arrow applied to a literal: its result, the parameter's
   variable bound to the literal's type. */
static mt_atom *applied(const mt_atom *arrow, const char *literal_type)
{
    mt_atom *type = S(literal_type);
    mt_bindings *b = mt_unify(mt_at(arrow, 1), type);
    require("the parameter unifies", b != NULL);
    mt_atom *out = mt_substitute(mt_at(arrow, 2), b);
    mt_bindings_free(b), mt_drop(type);
    return out;
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    signature table[] = {
        { "if", E("->", "Bool", "Atom", "Atom", V("t")) },
        { "let", E("->", "Atom", "%Undefined%", "Atom", "%Undefined%") },
        { "chain", E("->", "Atom", "Variable", "Atom", "%Undefined%") },
        { "quote", E("->", "Atom", "Atom") },
        { "collapse", E("->", "Atom", "Atom") },
        { "superpose", E("->", "Expression", "%Undefined%") },
        { "match", E("->", "SpaceType", "Atom", "Atom", "%Undefined%") },
        { "map-atom", E("->", "Expression", "Variable", "Atom", "Expression") },
        { "map-atom", E("->", "Expression", "Expression", "Expression") },
        { "car-atom", E("->", "Expression", "%Undefined%") },
        { "cdr-atom", E("->", "Expression", "Expression") },
        { "cons-atom", E("->", "%Undefined%", "%Undefined%", "Atom") },
        { "size-atom", E("->", "Expression", "Number") },
        { "index-atom", E("->", "%Undefined%", "Number", "Atom") },
        { "sort-atom", E("->", "%Undefined%", "Expression") },
        { "is-var", E("->", "Atom", "Bool") },
        { "repr", E("->", "%Undefined%", "String") },
        { "current-time", E("->", "Number") },
        { "new-state", E("->", V("t"), E("StateMonad", V("t"))) },
        { "change-state!", E("->", E("StateMonad", V("t")), V("t"), "Bool") },
        { "get-state", E("->", E("StateMonad", V("t")), V("t")) },
    };
    size_t n = sizeof table / sizeof *table;
    for (size_t i = 0; i < n; i++) {
        if (i + 1 < n && strcmp(table[i].name, table[i + 1].name) == 0) continue;
        mt_atom *arrows = arrows_of(table, n, table[i].name);
        bool several = mt_len(arrows) > 1;
        if (several) assert(answers_are(mt_eval(m, E("collapse", E("get-type", table[i].name))), E(arrows)) && table[i].name);
        else assert(answers_are(mt_eval(m, E("get-type", table[i].name)), E(mt_keep(mt_at(arrows, 0)))) && table[i].name), mt_drop(arrows);
    }
    mt_atom *state = arrows_of(table, n, "new-state");
    assert(answers_are(mt_eval(m, E("get-type", E("new-state", 5))), E(applied(mt_at(state, 0), "Number"))) && "a cell of a number");
    assert(answers_are(mt_eval(m, E("get-type", E("new-state", T("hi")))), E(applied(mt_at(state, 0), "String"))) && "a cell of a text");
    mt_drop(state);

    mt_atom *own = E(":", "program-own-type", "MyType");
    require("(: program-own-type MyType)", mt_add(m, mt_keep(own)));
    assert(answers_are(mt_eval(m, E("collapse", E("match", "&self", E(":", V("n"), V("t")), V("n")))), E(E(mt_keep(mt_at(own, 1)))))
           && "&self holds the program's declarations only");
    mt_drop(own);
    mt_atom *override = E(":", "car-atom", "MyOverride"), *pair = E("a", "b");
    require("(: car-atom MyOverride)", mt_add(m, mt_keep(override)));
    assert(answers_are(mt_eval(m, E("car-atom", mt_keep(pair))), E(mt_keep(mt_at(pair, 0)))) && "an operation still runs");
    mt_atom *surface = arrows_of(table, n, "car-atom");
    assert(answers_are(mt_eval(m, E("collapse", E("get-type", "car-atom"))), E(E(mt_keep(mt_at(override, 2)), mt_keep(mt_at(surface, 0)))))
           && "the program's declaration first");
    mt_drop(surface), mt_drop(override), mt_drop(pair);
    for (size_t i = 0; i < n; i++) mt_drop(table[i].arrow);
    mt_close(m);
    return 0;
}
