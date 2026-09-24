/* Purpose: an imported declaration is an atom in the space like any other.
 *   After lib_datastructures is imported, C reads the finger tree's
 *   declarations back with mt_match and holds get-type to them: a name
 *   answers its declared type, and an application answers the arrow's last
 *   type when it fits, as many arguments as parameters, each admitted: Atom
 *   admits anything, Expression an expression, and any other parameter a
 *   term declared of that type. An application that does not fit has no
 *   type at all. metatype() tells the nullary constructor, a symbol, from an
 *   applied one.
 * Guarantees: all ten claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

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
    metta *m = open_engine();
    require("import lib_datastructures", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_datastructures")))));
    const char *names[] = { "FTree", "FTEmpty", "FTSingle", "FTDeep" };
    for (size_t i = 0; i < 4; i++) {
        mt_atom *name = S(names[i]), *type = declared(m, name);
        require("a declaration in the space", type != NULL);
        check_answers(names[i], mt_eval(m, E("get-type", name)), type);
    }
    mt_atom *single = E("FTSingle", 1), *deep = E("FTDeep", E(1, 2), "FTEmpty", E(3, 4));
    check_answers("an applied constructor is a tree", mt_eval(m, E("collapse", E("get-type", mt_keep(single)))), application_types(m, single));
    check_answers("and so is a deep node", mt_eval(m, E("collapse", E("get-type", mt_keep(deep)))), application_types(m, deep));
    mt_atom *empty = S("FTEmpty");
    check_answers("a nullary constructor is a symbol", mt_eval(m, E("get-metatype", mt_keep(empty))), S(metatype(empty)));
    check_answers("an applied one an expression", mt_eval(m, E("get-metatype", mt_keep(single))), S(metatype(single)));
    mt_atom *overfull = E("FTSingle", 1, 2), *misfit = E("FTDeep", 1, "FTEmpty", E(3, 4));
    check_answers("an argument the arrow has no room for", mt_eval(m, E("collapse", E("get-type", mt_keep(overfull)))), application_types(m, overfull));
    check_answers("a number where a digit goes", mt_eval(m, E("collapse", E("get-type", mt_keep(misfit)))), application_types(m, misfit));
    mt_atom *held[] = { single, deep, empty, overfull, misfit };
    for (size_t i = 0; i < 5; i++) mt_drop(held[i]);
    return done(m);
}
