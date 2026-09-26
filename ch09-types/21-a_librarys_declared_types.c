/* Purpose: an imported declaration is an atom in the space like any other.
 *   After lib_datastructures is imported, C reads the finger tree's
 *   declarations back with mt_match and holds get-type to them: a name
 *   answers its declared type, and an application answers the arrow's last
 *   type when it fits, as many arguments as parameters, each admitted: Atom
 *   admits anything, Expression an expression, and any other parameter a
 *   term declared of that type. An application that does not fit has no
 *   type at all. metatype() tells the nullary constructor, a symbol, from an
 *   applied one.
 * Guarantees: all ten claims of the original hold
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

/* The metatype get-metatype answers for an atom of this kind: Symbol for a
   symbol and for a space, which the engine names by a symbol; Variable;
   Expression, the empty one included; Grounded for every value. */
static inline const char *metatype(const mt_atom *atom)
{
    switch (mt_kind_of(atom)) {
    case MT_SYMBOL:
    case MT_SPACE: return "Symbol";
    case MT_VARIABLE: return "Variable";
    case MT_EXPR: return "Expression";
    default: return "Grounded";
    }
}

/* The type a name is declared with in the space, read with mt_match. */
static mt_atom *declared(metta *m, const mt_atom *name)
{
    return mt_first(mt_eval(m, E("match", "&self", E(":", mt_keep(name), V("t")), V("t"))));
}

static bool admits(metta *m, const mt_atom *parameter, const mt_atom *argument)
{
    if (mt_kind_of(parameter) == MT_SYMBOL && strcmp(mt_name(parameter), "Atom") == 0) return true;
    if (mt_kind_of(parameter) == MT_SYMBOL && strcmp(mt_name(parameter), "Expression") == 0) return mt_kind_of(argument) == MT_EXPR;
    if (mt_kind_of(argument) != MT_SYMBOL) return false;
    mt_atom *type = declared(m, argument);
    bool fits = type && mt_eq(type, parameter);
    mt_drop(type);
    return fits;
}

/* An application's types: the arrow's last type when the call fits it, or
   none. */
static mt_atom *application_types(metta *m, const mt_atom *call)
{
    mt_atom *arrow = declared(m, mt_at(call, 0));
    bool fits = arrow && mt_kind_of(arrow) == MT_EXPR && mt_len(arrow) == mt_len(call) + 1;
    for (size_t i = 1; fits && i < mt_len(call); i++) fits = admits(m, mt_at(arrow, i), mt_at(call, i));
    mt_atom *out = fits ? E(mt_keep(mt_at(arrow, mt_len(arrow) - 1))) : mt_unit();
    mt_drop(arrow);
    return out;
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_datastructures", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_datastructures")))));
    const char *names[] = { "FTree", "FTEmpty", "FTSingle", "FTDeep" };
    for (size_t i = 0; i < 4; i++) {
        mt_atom *name = S(names[i]), *type = declared(m, name);
        require("a declaration in the space", type != NULL);
        assert(answers_are(mt_eval(m, E("get-type", name)), E(type)) && names[i]);
    }
    mt_atom *single = E("FTSingle", 1), *deep = E("FTDeep", E(1, 2), "FTEmpty", E(3, 4));
    assert(answers_are(mt_eval(m, E("collapse", E("get-type", mt_keep(single)))), E(application_types(m, single))) && "an applied constructor is a tree");
    assert(answers_are(mt_eval(m, E("collapse", E("get-type", mt_keep(deep)))), E(application_types(m, deep))) && "and so is a deep node");
    mt_atom *empty = S("FTEmpty");
    assert(answers_are(mt_eval(m, E("get-metatype", mt_keep(empty))), E(S(metatype(empty)))) && "a nullary constructor is a symbol");
    assert(answers_are(mt_eval(m, E("get-metatype", mt_keep(single))), E(S(metatype(single)))) && "an applied one an expression");
    mt_atom *overfull = E("FTSingle", 1, 2), *misfit = E("FTDeep", 1, "FTEmpty", E(3, 4));
    assert(answers_are(mt_eval(m, E("collapse", E("get-type", mt_keep(overfull)))), E(application_types(m, overfull))) && "an argument the arrow has no room for");
    assert(answers_are(mt_eval(m, E("collapse", E("get-type", mt_keep(misfit)))), E(application_types(m, misfit))) && "a number where a digit goes");
    mt_atom *held[] = { single, deep, empty, overfull, misfit };
    for (size_t i = 0; i < 5; i++) mt_drop(held[i]);
    mt_close(m);
    return 0;
}
