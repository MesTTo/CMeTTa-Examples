/* Purpose: documentation as atoms, held against a C description of the one
 *   documented function: its description, its parameters' and its return's,
 *   and its declared arrow. Two rules over that struct give every answer. As
 *   written, it is the @doc atom the space holds. Formally, each description
 *   gains an @type: the arrow's type at that position, or %Undefined% when
 *   none is known, which is how get-doc-atom, get-doc-single-atom,
 *   get-doc-function and get-doc-params differ only in the types they are
 *   handed. What a space defines, documents and leaves undocumented are set
 *   operations over C's tables, sorted with mt_order where the original sorts.
 * Guarantees: all fifteen claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

#define COUNT(array) (sizeof (array) / sizeof *(array))
enum { MOST = 8 };

typedef struct documented_function {
    const char *name, *desc, *returns;
    const char *params[MOST];
    size_t arity;
} documented_function;

static const documented_function TWICE = { "twice", "Doubles a number", "twice it", { "the number" }, 1 };
static const char *const TWICE_ARROW[] = { "Number", "Number" };   /* parameters, then the return */

/* The heads this space defines, and which of them carry a @doc. */
static const char *const DEFINED[] = { "twice", "undocumented-here" };
static const char *const WITH_DOC[] = { "twice" };

static bool listed(const char *name, const char *const *list, size_t n)
{
    for (size_t i = 0; i < n; i++)
        if (strcmp(list[i], name) == 0) return true;
    return false;
}

/* Names as the sorted expression the engine's sort-atom answers. */
static mt_atom *sorted(const char *const *names, size_t n)
{
    mt_atom *kids[MOST];
    for (size_t i = 0; i < n; i++) kids[i] = mt_sym(names[i]);
    qsort(kids, n, sizeof *kids, mt_order);
    return mt_exprv(n, kids);
}

static mt_atom *as_written(const documented_function *f)
{
    mt_atom *params[MOST];
    for (size_t i = 0; i < f->arity; i++) params[i] = E("@param", T(f->params[i]));
    return E("@doc", mt_sym(f->name), E("@desc", T(f->desc)), E("@params", mt_exprv(f->arity, params)),
             E("@return", T(f->returns)));
}

/* A type slot: the arrow's type at a position, or the gradual default. */
static mt_atom *type_at(const char *const *types, size_t i)
{
    return E("@type", types ? mt_sym(types[i]) : mt_sym("%Undefined%"));
}

/* get-doc-params: parameter descriptions, a return description and types in,
   the formal parameter list and the formal return out. */
static mt_atom *formal_params(const char *const *descs, size_t n, const char *returns, const char *const *types)
{
    mt_atom *params[MOST];
    for (size_t i = 0; i < n; i++) params[i] = E("@param", type_at(types, i), E("@desc", T(descs[i])));
    return E(mt_exprv(n, params), E("@return", type_at(types, n), E("@desc", T(returns))));
}

/* An arrow over types, the parameters' then the return's. */
static mt_atom *arrow_of(const char *const *types, size_t arity)
{
    mt_atom *parts[MOST + 1] = { mt_sym("->") };
    for (size_t i = 0; i <= arity; i++) parts[i + 1] = mt_sym(types[i]);
    return mt_exprv(arity + 2, parts);
}

/* The formal document for a function under an arrow, or under none. */
static mt_atom *formal(const documented_function *f, const char *const *arrow)
{
    mt_atom *pair = formal_params(f->params, f->arity, f->returns, arrow);
    mt_atom *arrow_type = arrow ? arrow_of(arrow, f->arity) : mt_sym("%Undefined%");
    mt_atom *doc = E("@doc-formal", E("@item", mt_sym(f->name)), E("@kind", "function"), E("@type", arrow_type),
                     E("@desc", T(f->desc)), E("@params", mt_keep(mt_at(pair, 0))), mt_keep(mt_at(pair, 1)));
    mt_drop(pair);
    return doc;
}

static mt_atom *named(mt_atom *goal) { return E("let", V("names"), E("collapse", goal), V("names")); }

int main(void)
{
    metta *m = open_engine();
    require("the @doc", mt_add(m, as_written(&TWICE)));
    require("its type", mt_add(m, E(":", "twice", arrow_of(TWICE_ARROW, TWICE.arity))));
    require("twice", mt_add(m, E("=", E("twice", V("x")), E("*", 2, V("x")))));
    require("undocumented-here", mt_add(m, E("=", E("undocumented-here", V("x")), V("x"))));

    const char *documented[MOST], *undocumented[MOST];
    size_t n_doc = 0, n_undoc = 0;
    for (size_t i = 0; i < COUNT(DEFINED); i++) {
        if (listed(DEFINED[i], WITH_DOC, COUNT(WITH_DOC))) documented[n_doc++] = DEFINED[i];
        else undocumented[n_undoc++] = DEFINED[i];
    }
    check_answers("every head defined here, sorted",
                  mt_eval(m, E("let", V("names"), E("collapse", E("defined-name")), E("sort-atom", V("names")))),
                  sorted(DEFINED, COUNT(DEFINED)));
    check_answers("the documented ones", mt_eval(m, named(E("documented"))), sorted(documented, n_doc));
    check_answers("the undocumented ones", mt_eval(m, named(E("undocumented-space", "&self"))),
                  sorted(undocumented, n_undoc));
    check_answers("the documented ones, space named", mt_eval(m, named(E("documented-space", "&self"))),
                  sorted(documented, n_doc));

    /* A space holding one undocumented equation. */
    static const char *const F[] = { "f" };
    mt_space *empty = mt_space_open(m, "&empty");
    require("a space of its own", empty && mt_add(empty, E("=", E("f", V("x")), V("x"))));
    check_answers("it documents nothing", mt_eval(m, named(E("documented-space", mt_spaceref("&empty")))),
                  sorted(NULL, 0));
    check_answers("and leaves f undocumented", mt_eval(m, named(E("undocumented-space", mt_spaceref("&empty")))),
                  sorted(F, 1));
    mt_space_close(empty);

    /* The three shapes of one document, and the step they share. */
    check_answers("as written", mt_eval(m, E("get-doc-space", "&self", "twice")), as_written(&TWICE));
    check_answers("formal, every type gradual", mt_eval(m, E("get-doc-atom", "&self", "twice")), formal(&TWICE, NULL));
    check_answers("formal, under the declared arrow", mt_eval(m, E("get-doc-single-atom", "&self", "twice")),
                  formal(&TWICE, TWICE_ARROW));
    mt_atom *by_hand = formal(&TWICE, TWICE_ARROW), *declared = formal(&TWICE, TWICE_ARROW);
    check_answers("an arrow given is the arrow declared",
                  mt_eval(m, E("==", E("get-doc-function", "&self", "twice", arrow_of(TWICE_ARROW, 1)),
                               E("get-doc-single-atom", "&self", "twice"))),
                  B(mt_eq(by_hand, declared)));
    mt_drop(by_hand);
    mt_drop(declared);
    static const char *const STRINGS[] = { "String", "String" };
    check_answers("and any arrow given is used", mt_eval(m, E("get-doc-function", "&self", "twice", arrow_of(STRINGS, 1))),
                  formal(&TWICE, STRINGS));
    static const char *const NUMBERS[] = { "Number", "Number" }, *const BOOL[] = { "Bool" };
    check_answers("the shared step",
                  mt_eval(m, E("get-doc-params", E(E("@param", T("the number"))), E("@return", T("twice it")),
                               E("Number", "Number"))),
                  formal_params(TWICE.params, 1, "twice it", NUMBERS));
    check_answers("with no parameters",
                  mt_eval(m, E("get-doc-params", mt_unit(), E("@return", T("nothing to say")), E("Bool"))),
                  formal_params(NULL, 0, "nothing to say", BOOL));

    check_answers("help! prints and answers the unit", mt_eval(m, E("help!", "twice")), mt_unit());
    check_answers("for an undocumented head too", mt_eval(m, E("help!", "undocumented-here")), mt_unit());
    return done(m);
}
