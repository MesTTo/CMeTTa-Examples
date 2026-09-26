/* Purpose: when the cons happens. One computation, (cons 42 $arg), defined
 *   three ways that differ only in when it runs: runtime42 has no translator
 *   rule, so its call runs at run time; compileeval42 has one, so the
 *   compiler expands the call and then evaluates the expansion; compile42
 *   wraps its body in noeval, so the expansion is handed back as data. C
 *   keeps the three as rows of a table, builds each equation from its row
 *   and registers a rule for the rows that ask for one, and every call must
 *   answer what C's own cons builds, the head before the list's items.
 * Guarantees: all three claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>

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

/* One definition of (cons 42 $arg): its name, whether a translator rule
   expands its calls, and whether its expansion is handed back as data. */
typedef struct definition {
    const char *name;
    bool rule, noeval;
} definition;

static const definition definitions[] = {
    { "runtime42", false, false },
    { "compileeval42", true, false },
    { "compile42", true, true },
};
#define DEFINITIONS (sizeof definitions / sizeof *definitions)

enum { HEAD = 42 };

/* C's cons: HEAD before the items of LIST, which it borrows. */
static mt_atom *cons(int64_t head, const mt_atom *list)
{
    size_t n = mt_len(list);
    mt_atom **items = malloc((n + 1) * sizeof *items);
    require("room for a cons", items != NULL);
    items[0] = N(head);
    for (size_t i = 0; i < n; i++) items[i + 1] = mt_keep(mt_at(list, i));
    mt_atom *consed = mt_exprv(n + 1, items);
    free(items);
    return consed;
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    for (size_t i = 0; i < DEFINITIONS; i++) {
        mt_atom *body = E("cons", HEAD, V("arg"));
        require(definitions[i].name,
                mt_add(m, E("=", E(definitions[i].name, V("arg")), definitions[i].noeval ? E("noeval", body) : body)));
    }
    for (size_t i = 0; i < DEFINITIONS; i++)
        if (definitions[i].rule)
            require("register its rule", mt_one_truth(mt_eval(m, E("add-translator-rule!", definitions[i].name))));

    mt_atom *list = E(43);
    for (size_t i = 0; i < DEFINITIONS; i++)
        assert(answers_are(mt_eval(m, E(definitions[i].name, mt_keep(list))), E(cons(HEAD, list))) && definitions[i].name);
    mt_drop(list);
    mt_close(m);
    return 0;
}
