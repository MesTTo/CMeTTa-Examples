/* Purpose: one implementation under two overload declarations. C declares
 *   both arrows from a table, get-type answers the table in order, and the
 *   single identity equation answers each argument, a number and a text, as
 *   itself.
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

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    const char *types[] = { "Number", "String" };
    mt_atom *arrows[2];
    for (size_t i = 0; i < 2; i++) {
        arrows[i] = E("->", types[i], types[i]);
        require("declare an overload", mt_add(m, E(":", "compiled-identity", mt_keep(arrows[i]))));
    }
    require("(= (compiled-identity $value) $value)", mt_add(m, E("=", E("compiled-identity", V("value")), V("value"))));
    assert(answers_are(mt_eval(m, E("collapse", E("get-type", "compiled-identity"))), E(mt_exprv(2, arrows))) && "both overloads");
    mt_atom *values[] = { N(7), T("word") };
    for (size_t i = 0; i < 2; i++) {
        assert(answers_are(mt_eval(m, E("compiled-identity", mt_keep(values[i]))), E(mt_keep(values[i]))) && (i ? "a text is itself" : "a number is itself"));
        mt_drop(values[i]);
    }
    mt_close(m);
    return 0;
}
