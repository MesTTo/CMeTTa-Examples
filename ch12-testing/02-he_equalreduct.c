/* Purpose: lib_he's small helpers, each held against C: id answers its
 *   argument, =alpha is mt_alpha_eq, and if-equal picks its branch by mt_eq.
 * Guarantees: all four claims of the original hold
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
    require("import lib_he", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_he")))));
    require("(= (add 1 2) 3)", mt_add(m, E("=", E("add", 1, 2), 3)));
    assert(answers_are(mt_eval(m, E("id", 5)), E(N(5))) && "id");
    mt_atom *pairs[][2] = { { E("Father", V("X")), E("Father", V("Y")) }, { E("Father", V("X")), E("Son", V("X")) } };
    for (size_t i = 0; i < 2; i++) {
        assert(answers_are(mt_eval(m, E("=alpha", mt_keep(pairs[i][0]), mt_keep(pairs[i][1]))), E(B(mt_alpha_eq(pairs[i][0], pairs[i][1])))) && (i ? "not alpha-equal" : "alpha-equal"));
        mt_drop(pairs[i][0]), mt_drop(pairs[i][1]);
    }
    mt_atom *one = N(1);
    assert(answers_are(mt_eval(m, E("if-equal", 1, 1, T("Equal"), T("Not Equal"))), E(T(mt_eq(one, one) ? "Equal" : "Not Equal"))) && "if-equal");
    mt_drop(one);
    mt_close(m);
    return 0;
}
