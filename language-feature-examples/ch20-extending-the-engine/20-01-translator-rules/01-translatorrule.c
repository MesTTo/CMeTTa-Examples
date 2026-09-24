/* Purpose: when the cons happens. One computation, (cons 42 $arg), defined
 *   three ways that differ only in when it runs: runtime42 has no translator
 *   rule, so its call runs at run time; compileeval42 has one, so the
 *   compiler expands the call and then evaluates the expansion; compile42
 *   wraps its body in noeval, so the expansion is handed back as data. C
 *   keeps the three as rows of a table, builds each equation from its row
 *   and registers a rule for the rows that ask for one, and every call must
 *   answer what C's own cons builds, the head before the list's items.
 * Guarantees: all three claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

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
    metta *m = open_engine();
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
        check_answers(definitions[i].name, mt_eval(m, E(definitions[i].name, mt_keep(list))), cons(HEAD, list));
    mt_drop(list);
    return done(m);
}
