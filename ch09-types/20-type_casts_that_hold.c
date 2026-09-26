/* Purpose: whether an atom has exactly a type in a space. C keeps each
 *   space's declarations as a table and answers the question itself: a
 *   literal carries its own type, a declared name has the types its space
 *   declares, and an undeclared name has %Undefined% alone; the cast holds
 *   when the type asked is identical to one of them. type-cast answers the
 *   atom where the cast holds and (Error atom BadType) where it does not.
 * Guarantees: all seventeen claims of the original hold, with its one
 *   unasserted form checked as well [tested 2026-09-27T00:35:58+10:00:
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
    const char *space, *term;
    mt_atom *type;
} declaration;

/* Whether `type` is one of x's types in `space`. */
static bool holds(const declaration *table, size_t n, const mt_atom *x, const mt_atom *type, const char *space)
{
    if (mt_kind_of(x) == MT_TEXT || mt_kind_of(x) == MT_INT) {
        mt_atom *own = S(mt_kind_of(x) == MT_TEXT ? "String" : "Number");
        bool same = mt_eq(own, type);
        mt_drop(own);
        return same;
    }
    bool declared = false, same = false;
    for (size_t i = 0; i < n; i++)
        if (strcmp(table[i].space, space) == 0 && strcmp(table[i].term, mt_name(x)) == 0) declared = true, same |= mt_eq(table[i].type, type);
    if (declared) return same;
    mt_atom *undefined = S("%Undefined%");
    same = mt_eq(undefined, type);
    mt_drop(undefined);
    return same;
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    declaration table[] = {
        { "&self", "five", S("Number") }, { "&self", "greeting", S("String") }, { "&self", "twice", E("->", "Number", "Number") },
        { "&other", "five", S("String") },
    };
    size_t n = sizeof table / sizeof *table;
    for (size_t i = 0; i < 3; i++) require("declare a type", mt_add(m, E(":", table[i].term, mt_keep(table[i].type))));
    require("(= (twice $x) (* 2 $x))", mt_add(m, E("=", E("twice", V("x")), E("*", 2, V("x")))));

    const struct { mt_atom *x, *type; const char *space; } questions[] = {
        { S("five"), S("Number"), "&self" }, { S("five"), S("Bool"), "&self" }, { S("greeting"), S("String"), "&self" },
        { N(1), S("Number"), "&self" }, { T("text"), S("String"), "&self" }, { N(1), S("String"), "&self" },
        { S("twice"), E("->", "Number", "Number"), "&self" }, { S("twice"), E("->", "String", "String"), "&self" },
        { S("five"), S("%Undefined%"), "&self" }, { S("undeclared-name"), S("Number"), "&self" }, { S("undeclared-name"), S("String"), "&self" },
        { S("undeclared-name"), S("%Undefined%"), "&self" },
    };
    for (size_t i = 0; i < sizeof questions / sizeof *questions; i++) {
        char *label = mt_show_dup(questions[i].x);
        assert(answers_are(mt_eval(m, E("type-cast-holds", mt_keep(questions[i].x), mt_keep(questions[i].type), questions[i].space)), E(B(holds(table, n, questions[i].x, questions[i].type, questions[i].space))))
               && label);
        mt_free(label), mt_drop(questions[i].x), mt_drop(questions[i].type);
    }

    assert(answers_are(mt_eval(m, E("add-atom", "&other", E(":", table[3].term, mt_keep(table[3].type)))), E(B(true))) && "another space's declaration");
    mt_atom *five = S("five"), *string = S("String"), *boolean = S("Bool"), *one = N(1), *number = S("Number");
    assert(answers_are(mt_eval(m, E("type-cast-holds", mt_keep(five), mt_keep(string), "&other")), E(B(holds(table, n, five, string, "&other")))) && "holds there");
    assert(answers_are(mt_eval(m, E("type-cast-holds", mt_keep(five), mt_keep(string), "&self")), E(B(holds(table, n, five, string, "&self")))) && "and not here");
    bool one_holds = holds(table, n, one, number, "&self"), five_holds = holds(table, n, five, boolean, "&self");
    assert(answers_are(mt_eval(m, E("type-cast", mt_keep(one), mt_keep(number), "&self")), E(one_holds ? mt_keep(one) : E("Error", mt_keep(one), "BadType")))
           && "a cast that holds answers the atom");
    assert(answers_are(mt_eval(m, E("type-cast", mt_keep(five), mt_keep(boolean), "&self")), E(five_holds ? mt_keep(five) : E("Error", mt_keep(five), "BadType")))
           && "one that does not answers an error");
    assert(answers_are(mt_eval(m, E("if", E("type-cast-holds", mt_keep(five), mt_keep(boolean), "&self"), "yes", "no")), E(S(five_holds ? "yes" : "no")))
           && "the predicate composes with if");
    mt_atom *held[] = { five, string, boolean, one, number };
    for (size_t i = 0; i < 5; i++) mt_drop(held[i]);
    for (size_t i = 0; i < n; i++) mt_drop(table[i].type);
    mt_close(m);
    return 0;
}
