/* Purpose: lib_he's exact type comparison and the casts it refuses. C
 *   decides each answer. is-function holds of an arrow of one argument.
 *   type-cast admits an atom whose type C knows equals the one asked: its
 *   declared type from C's table, Number for a number, or the metatype
 *   get-metatype names for its kind; anything else is (Error atom BadType).
 *   match-types is structural equality on the two types as written, so it
 *   neither treats Atom as a wildcard nor binds (List $x). The pair halves
 *   are C's own first and second, and match-type-or answers True where its
 *   types are equal and its folded value otherwise.
 * Guarantees: all fourteen claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

static const struct { const char *atom, *type; } declared[] = { { "type1", "Type" }, { "A", "type1" } };
#define DECLARED (sizeof declared / sizeof *declared)

static bool is_function(const mt_atom *type)
{
    return mt_kind_of(type) == MT_EXPR && mt_len(type) == 3 && mt_kind_of(mt_at(type, 0)) == MT_SYMBOL &&
           strcmp(mt_name(mt_at(type, 0)), "->") == 0;
}

static bool is_number(const mt_atom *atom)
{
    switch (mt_kind_of(atom)) {
    case MT_INT: case MT_FLOAT: case MT_BIGINT: case MT_RATIONAL: case MT_BIGRATIONAL: return true;
    default: return false;
    }
}

/* Whether C knows ATOM has TYPE. */
static bool has_type(const mt_atom *atom, const char *type)
{
    if (strcmp(metatype(atom), type) == 0) return true;
    if (is_number(atom) && strcmp(type, "Number") == 0) return true;
    if (mt_kind_of(atom) == MT_SYMBOL)
        for (size_t i = 0; i < DECLARED; i++)
            if (strcmp(declared[i].atom, mt_name(atom)) == 0 && strcmp(declared[i].type, type) == 0) return true;
    return false;
}

/* type-cast's answer: the atom, or (Error atom BadType). TAKES atom. */
static mt_atom *cast(mt_atom *atom, const char *type)
{
    return has_type(atom, type) ? atom : E("Error", atom, "BadType");
}

int main(void)
{
    metta *m = open_engine();
    require("import lib_he", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_he")))));
    mt_atom *types[] = { E("->", "Atom", "Atom"), S("Atom") };
    for (size_t i = 0; i < sizeof types / sizeof *types; i++) {
        bool function = is_function(types[i]);
        check_answers("an arrow of one argument is a function", mt_eval(m, E("is-function", types[i])), B(function));
    }

    for (size_t i = 0; i < DECLARED; i++) require("a declaration", mt_add(m, E(":", declared[i].atom, declared[i].type)));
    const struct { mt_atom *atom; const char *type; } casts[] = {
        { S("A"), "type1" }, { N(1), "type1" }, { S("A"), "Symbol" }, { N(1), "Number" }, { S("B"), "type1" },
    };
    for (size_t i = 0; i < sizeof casts / sizeof *casts; i++)
        check_answers("type-cast admits exactly the types C knows",
                      mt_eval(m, E("type-cast", mt_keep(casts[i].atom), casts[i].type, mt_spaceref("&self"))),
                      cast(casts[i].atom, casts[i].type));

    mt_atom *matched = T("Matched!"), *missed = T("Didn't match");
    const struct { mt_atom *left, *right; } compared[] = {
        { S("Atom"), S("Atom") }, { S("Atom"), S("Number") }, { S("Bool"), S("Number") },
        { E("List", V("x")), E("List", "Number") },
    };
    for (size_t i = 0; i < sizeof compared / sizeof *compared; i++) {
        bool same = mt_eq(compared[i].left, compared[i].right);
        check_answers("match-types is equality as written",
                      mt_eval(m, E("match-types", compared[i].left, compared[i].right, mt_keep(matched), mt_keep(missed))),
                      mt_keep(same ? matched : missed));
    }
    mt_drop(matched), mt_drop(missed);

    mt_atom *pair = E("A", "B");
    check_answers("the first of a pair", mt_eval(m, E("first-from-pair", mt_keep(pair))), mt_keep(mt_at(pair, 0)));
    check_answers("and the second", mt_eval(m, E("second-from-pair", mt_keep(pair))), mt_keep(mt_at(pair, 1)));
    mt_drop(pair);

    mt_atom *folded = B(true), *number = S("Number"), *boolean = S("Bool");
    mt_atom *want = mt_eq(number, boolean) ? B(true) : mt_keep(folded);
    check_answers("match-type-or keeps its folded value when the types differ",
                  mt_eval(m, E("match-type-or", folded, number, boolean)), want);
    return done(m);
}
