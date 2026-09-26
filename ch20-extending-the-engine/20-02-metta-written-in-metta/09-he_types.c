/* Purpose: lib_he's exact type comparison and the casts it refuses. C
 *   decides each answer. is-function holds of an arrow of one argument.
 *   type-cast admits an atom whose type C knows equals the one asked: its
 *   declared type from C's table, Number for a number, or the metatype
 *   get-metatype names for its kind; anything else is (Error atom BadType).
 *   match-types is structural equality on the two types as written, so it
 *   neither treats Atom as a wildcard nor binds (List $x). The pair halves
 *   are C's own first and second, and match-type-or answers True where its
 *   types are equal and its folded value otherwise.
 * Guarantees: all fourteen claims of the original hold
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
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_he", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_he")))));
    mt_atom *types[] = { E("->", "Atom", "Atom"), S("Atom") };
    for (size_t i = 0; i < sizeof types / sizeof *types; i++) {
        bool function = is_function(types[i]);
        assert(answers_are(mt_eval(m, E("is-function", types[i])), E(B(function))) && "an arrow of one argument is a function");
    }

    for (size_t i = 0; i < DECLARED; i++) require("a declaration", mt_add(m, E(":", declared[i].atom, declared[i].type)));
    const struct { mt_atom *atom; const char *type; } casts[] = {
        { S("A"), "type1" }, { N(1), "type1" }, { S("A"), "Symbol" }, { N(1), "Number" }, { S("B"), "type1" },
    };
    for (size_t i = 0; i < sizeof casts / sizeof *casts; i++)
        assert(answers_are(mt_eval(m, E("type-cast", mt_keep(casts[i].atom), casts[i].type, mt_spaceref("&self"))), E(cast(casts[i].atom, casts[i].type)))
               && "type-cast admits exactly the types C knows");

    mt_atom *matched = T("Matched!"), *missed = T("Didn't match");
    const struct { mt_atom *left, *right; } compared[] = {
        { S("Atom"), S("Atom") }, { S("Atom"), S("Number") }, { S("Bool"), S("Number") },
        { E("List", V("x")), E("List", "Number") },
    };
    for (size_t i = 0; i < sizeof compared / sizeof *compared; i++) {
        bool same = mt_eq(compared[i].left, compared[i].right);
        assert(answers_are(mt_eval(m, E("match-types", compared[i].left, compared[i].right, mt_keep(matched), mt_keep(missed))), E(mt_keep(same ? matched : missed)))
               && "match-types is equality as written");
    }
    mt_drop(matched), mt_drop(missed);

    mt_atom *pair = E("A", "B");
    assert(answers_are(mt_eval(m, E("first-from-pair", mt_keep(pair))), E(mt_keep(mt_at(pair, 0)))) && "the first of a pair");
    assert(answers_are(mt_eval(m, E("second-from-pair", mt_keep(pair))), E(mt_keep(mt_at(pair, 1)))) && "and the second");
    mt_drop(pair);

    mt_atom *folded = B(true), *number = S("Number"), *boolean = S("Bool");
    mt_atom *want = mt_eq(number, boolean) ? B(true) : mt_keep(folded);
    assert(answers_are(mt_eval(m, E("match-type-or", folded, number, boolean)), E(want))
           && "match-type-or keeps its folded value when the types differ");
    mt_close(m);
    return 0;
}
