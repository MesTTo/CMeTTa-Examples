/* Purpose: lib_doc's documentation as data, held against one C table. Each
 *   function is a row carrying its documentation, a description and, when
 *   written, its parameters and what it returns; the rows build the @doc
 *   atoms the space holds and the answer get-doc must give for each name. A
 *   name with no row has no documentation, and what the program leaves
 *   undocumented is a set difference over the table, the functions defined
 *   less the ones with a row: empty here.
 * Guarantees: all five claims of the original hold
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

typedef struct documentation {
    const char *name, *desc;
    const char *params[2];   /* NULL when not written */
    const char *returns;     /* NULL when not written */
} documentation;

static const documentation DOCUMENTED[] = {
    { "greet", "Greets somebody by name", { NULL, NULL }, NULL },
    { "add-two", "Adds two numbers", { "the first", "the second" }, "their sum" },
};

/* The functions this program defines, documented or not. */
static const char *const DEFINED[] = { "greet", "add-two" };

/* The @doc atom for a row: two, three or four parts, however much was
   written. */
static mt_atom *doc_atom(const documentation *d)
{
    mt_atom *parts[4] = { mt_sym(d->name), E("@desc", T(d->desc)) };
    size_t n = 2;
    if (d->params[0]) {
        mt_atom *params[COUNT(d->params)];
        size_t k = 0;
        while (k < COUNT(d->params) && d->params[k]) {
            params[k] = E("@param", T(d->params[k]));
            k++;
        }
        parts[n++] = E("@params", mt_exprv(k, params));
    }
    if (d->returns) parts[n++] = E("@return", T(d->returns));
    mt_atom *doc[5] = { mt_sym("@doc") };
    memcpy(doc + 1, parts, n * sizeof *parts);
    return mt_exprv(n + 1, doc);
}

static const documentation *row_of(const char *name)
{
    for (size_t i = 0; i < COUNT(DOCUMENTED); i++)
        if (strcmp(DOCUMENTED[i].name, name) == 0) return &DOCUMENTED[i];
    return NULL;
}

/* get-doc's answers for a name: its row's atom, or none. */
static void check_doc(metta *m, const char *claim, const char *name)
{
    const documentation *d = row_of(name);
    mt_answers *answers = mt_eval(m, E("get-doc", mt_sym(name)));
    if (d) assert(answers_are(answers, E(doc_atom(d))) && claim);
    else assert(!mt_first(answers) && mt_ok() && claim);
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_doc", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_doc")))));
    for (size_t i = 0; i < COUNT(DOCUMENTED); i++)
        require(DOCUMENTED[i].name, mt_add(m, doc_atom(&DOCUMENTED[i])));
    require("greet", mt_add(m, E("=", E("greet", V("who")), V("who"))));
    require("add-two", mt_add(m, E("=", E("add-two", V("a"), V("b")), E("+", V("a"), V("b")))));

    check_doc(m, "a one-part doc comes back as written", "greet");
    check_doc(m, "so does a four-part one", "add-two");
    check_doc(m, "an undocumented name answers nothing", "greet-nobody");
    check_doc(m, "nor does a name nobody defined", "missing");

    size_t gaps = 0;
    for (size_t i = 0; i < COUNT(DEFINED); i++) gaps += row_of(DEFINED[i]) == NULL;
    mt_answers *undocumented = mt_eval(m, E("undocumented"));
    if (gaps == 0) assert(!mt_first(undocumented) && mt_ok() && "nothing defined is undocumented");
    else assert(false && "the table says what is undocumented");
    mt_close(m);
    return 0;
}
