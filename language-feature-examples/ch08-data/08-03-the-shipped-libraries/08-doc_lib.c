/* Purpose: lib_doc's documentation as data, held against one C table. Each
 *   function is a row carrying its documentation, a description and, when
 *   written, its parameters and what it returns; the rows build the @doc
 *   atoms the space holds and the answer get-doc must give for each name. A
 *   name with no row has no documentation, and what the program leaves
 *   undocumented is a set difference over the table, the functions defined
 *   less the ones with a row: empty here.
 * Guarantees: all five claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

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
    if (d) check_answers(claim, answers, doc_atom(d));
    else check_none(claim, answers);
}

int main(void)
{
    metta *m = open_engine();
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
    if (gaps == 0) check_none("nothing defined is undocumented", undocumented);
    else check("the table says what is undocumented", false);
    return done(m);
}
