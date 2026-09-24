/* Purpose: a type is a declaration in the space. C keeps each (: term type)
 *   in a table and adds it in order, and get-type of a symbol answers every
 *   type the table gives it, in order, or %Undefined% when it gives none. A
 *   number's type is Number and a text's String, whatever the table says; a
 *   variable's is another variable; an expression's is the expression of its
 *   parts' types. An arrow types an application: testx's (-> $a $b $a) binds
 *   $a to its first argument's type. mid's let unifies (a b) with its
 *   argument, and testf's one equation rewrites at to t.
 * Guarantees: all thirteen claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

/* The concrete declarations, (: term type), in the original's order. */
static const struct { const char *term, *type; } declared[] = {
    { "a", "A" }, { "b", "B" }, { "A", "Type" }, { "x", "Letter" }, { "x", "Buchstabe" },
};
#define DECLARED (sizeof declared / sizeof *declared)

/* get-type's answers for a symbol: each type the table declares for it, in
   order, or %Undefined%. `out` has room for DECLARED + 1. */
static size_t types_of(const char *term, mt_atom **out)
{
    size_t n = 0;
    for (size_t i = 0; i < DECLARED; i++)
        if (strcmp(declared[i].term, term) == 0) out[n++] = S(declared[i].type);
    if (n == 0) out[n++] = S("%Undefined%");
    return n;
}

/* A literal carries its type: Number for a number, String for a text. */
static mt_atom *literal_type(const mt_atom *x) { return S(mt_kind_of(x) == MT_TEXT ? "String" : "Number"); }

static mt_atom *first_type(const char *term)
{
    mt_atom *types[DECLARED + 1];
    size_t n = types_of(term, types);
    for (size_t i = 1; i < n; i++) mt_drop(types[i]);
    return types[0];
}

int main(void)
{
    metta *m = open_engine();
    for (size_t i = 0; i < DECLARED; i++) require("declare a type", mt_add(m, E(":", declared[i].term, declared[i].type)));

    check_answers("a variable's type is a variable", mt_eval(m, E("get-type", V("a"))), V("z"));
    const char *symbols[] = { "a", "b", "c", "A", "B" };
    for (size_t i = 0; i < sizeof symbols / sizeof *symbols; i++) {
        mt_atom *types[DECLARED + 1];
        size_t n = types_of(symbols[i], types);
        check_answers_(symbols[i], mt_eval(m, E("get-type", symbols[i])), n, types);
    }
    check_answers("an expression's type is its parts' types", mt_eval(m, E("get-type", E("a", "b"))), E(first_type("a"), first_type("b")));
    mt_atom *literals[] = { N(42), T("42") };
    for (size_t i = 0; i < 2; i++)
        check_answers(i ? "a text is a String" : "a number is a Number", mt_eval(m, E("get-type", mt_keep(literals[i]))), literal_type(literals[i]));
    mt_atom *x_types[DECLARED + 1];
    size_t x_count = types_of("x", x_types);
    check_answers("two declarations, two types", mt_eval(m, E("collapse", E("get-type", "x"))), mt_exprv(x_count, x_types));

    /* Function types: an arrow per function. */
    require("(: mid (-> $a $a))", mt_add(m, E(":", "mid", E("->", V("a"), V("a")))));
    require("(= (mid $x) (let (a b) $x $x))", mt_add(m, E("=", E("mid", V("x")), E("let", E("a", "b"), V("x"), V("x")))));
    check_answers("let unifies (a b) with ($a b)", mt_eval(m, E("mid", E(V("a"), "b"))), E("a", "b"));
    require("(: testx (-> $a $b $a))", mt_add(m, E(":", "testx", E("->", V("a"), V("b"), V("a")))));
    check_answers("$a is bound by the first argument", mt_eval(m, E("get-type", E("testx", mt_keep(literals[0]), T("f")))), literal_type(literals[0]));

    /* Non-deterministic types. */
    const struct { const char *term, *type; } more[] = { { "at", "A" }, { "at", "T" }, { "t", "T" } };
    for (size_t i = 0; i < 3; i++) require("declare a type", mt_add(m, E(":", more[i].term, more[i].type)));
    require("(: testf (-> $a $a))", mt_add(m, E(":", "testf", E("->", V("a"), V("a")))));
    require("(= (testf at) t)", mt_add(m, E("=", E("testf", "at"), "t")));
    check_answers("testf's equation", mt_eval(m, E("testf", "at")), S("t"));

    mt_drop(literals[0]), mt_drop(literals[1]);
    return done(m);
}
