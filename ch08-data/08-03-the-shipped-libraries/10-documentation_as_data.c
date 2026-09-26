/* Purpose: documentation as atoms, held against a C description of the one
 *   documented function: its description, its parameters' and its return's,
 *   and its declared arrow. Two rules over that struct give every answer. As
 *   written, it is the @doc atom the space holds. Formally, each description
 *   gains an @type: the arrow's type at that position, or %Undefined% when
 *   none is known, which is how get-doc-atom, get-doc-single-atom,
 *   get-doc-function and get-doc-params differ only in the types they are
 *   handed. What a space defines, documents and leaves undocumented are set
 *   operations over C's tables, sorted with mt_order where the original sorts.
 * Guarantees: all fifteen claims of the original hold
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
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
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
    assert(answers_are(mt_eval(m, E("let", V("names"), E("collapse", E("defined-name")), E("sort-atom", V("names")))), E(sorted(DEFINED, COUNT(DEFINED))))
           && "every head defined here, sorted");
    assert(answers_are(mt_eval(m, named(E("documented"))), E(sorted(documented, n_doc))) && "the documented ones");
    assert(answers_are(mt_eval(m, named(E("undocumented-space", "&self"))), E(sorted(undocumented, n_undoc)))
           && "the undocumented ones");
    assert(answers_are(mt_eval(m, named(E("documented-space", "&self"))), E(sorted(documented, n_doc)))
           && "the documented ones, space named");

    /* A space holding one undocumented equation. */
    static const char *const F[] = { "f" };
    mt_space *empty = mt_space_open(m, "&empty");
    require("a space of its own", empty && mt_add(empty, E("=", E("f", V("x")), V("x"))));
    assert(answers_are(mt_eval(m, named(E("documented-space", mt_spaceref("&empty")))), E(sorted(NULL, 0)))
           && "it documents nothing");
    assert(answers_are(mt_eval(m, named(E("undocumented-space", mt_spaceref("&empty")))), E(sorted(F, 1)))
           && "and leaves f undocumented");
    mt_space_close(empty);

    /* The three shapes of one document, and the step they share. */
    assert(answers_are(mt_eval(m, E("get-doc-space", "&self", "twice")), E(as_written(&TWICE))) && "as written");
    assert(answers_are(mt_eval(m, E("get-doc-atom", "&self", "twice")), E(formal(&TWICE, NULL))) && "formal, every type gradual");
    assert(answers_are(mt_eval(m, E("get-doc-single-atom", "&self", "twice")), E(formal(&TWICE, TWICE_ARROW)))
           && "formal, under the declared arrow");
    mt_atom *by_hand = formal(&TWICE, TWICE_ARROW), *declared = formal(&TWICE, TWICE_ARROW);
    assert(answers_are(mt_eval(m, E("==", E("get-doc-function", "&self", "twice", arrow_of(TWICE_ARROW, 1)),
                                    E("get-doc-single-atom", "&self", "twice"))), E(B(mt_eq(by_hand, declared))))
           && "an arrow given is the arrow declared");
    mt_drop(by_hand);
    mt_drop(declared);
    static const char *const STRINGS[] = { "String", "String" };
    assert(answers_are(mt_eval(m, E("get-doc-function", "&self", "twice", arrow_of(STRINGS, 1))), E(formal(&TWICE, STRINGS)))
           && "and any arrow given is used");
    static const char *const NUMBERS[] = { "Number", "Number" }, *const BOOL[] = { "Bool" };
    assert(answers_are(mt_eval(m, E("get-doc-params", E(E("@param", T("the number"))), E("@return", T("twice it")),
                                    E("Number", "Number"))), E(formal_params(TWICE.params, 1, "twice it", NUMBERS)))
           && "the shared step");
    assert(answers_are(mt_eval(m, E("get-doc-params", mt_unit(), E("@return", T("nothing to say")), E("Bool"))), E(formal_params(NULL, 0, "nothing to say", BOOL)))
           && "with no parameters");

    assert(answers_are(mt_eval(m, E("help!", "twice")), E(mt_unit())) && "help! prints and answers the unit");
    assert(answers_are(mt_eval(m, E("help!", "undocumented-here")), E(mt_unit())) && "for an undocumented head too");
    mt_close(m);
    return 0;
}
