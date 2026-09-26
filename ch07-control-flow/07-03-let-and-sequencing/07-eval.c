/* Purpose: reading a body, then running it. Matching f's equation returns
 *   its body, specialised to the call, as data; C holds that atom and runs
 *   it with mt_eval, which is what eval is. evalCustom emulates eval by
 *   storing the body as myfunc, reducing it and taking it back out, and C's
 *   own statements do the same three steps.
 * Guarantees: both claims of the original hold, and the C emulation agrees
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
    require("f", mt_add(m, E("=", E("f", V("L"), V("a"), V("b")),
                            E("let", V("result"), E("+", V("a"), V("b")), E("append", E(V("result")), V("L"))))));
    require("evalCustom", mt_add(m, E("=", E("evalCustom", V("body")),
                                     E("let*", E(E(V("a"), E("add-atom", "&self", E("=", E("myfunc"), V("body")))),
                                                 E(V("res"), E("reduce", E("myfunc"))),
                                                 E(V("r"), E("remove-atom", "&self", E("=", E("myfunc"), V("body"))))),
                                       V("res")))));

    mt_atom *body = NULL;
    mt_rows (row, mt_match(m, E("=", E("f", E(42), 40.7, 2), V("x")))) {
        mt_drop(body);
        body = mt_keep(mt_bound(row, "x"));
    }
    require("the specialised body", body != NULL);
    assert(answers_are(mt_eval(m, mt_keep(body)), E(E(42.7, 42))) && "running the body is mt_eval");
    assert(answers_are(mt_eval(m, E("evalCustom", mt_keep(body))), E(E(42.7, 42))) && "evalCustom emulates it");

    require("store (= (myfunc) body)", mt_add(m, E("=", E("myfunc"), mt_keep(body))));
    assert(answers_are(mt_eval(m, E("myfunc")), E(E(42.7, 42))) && "so do C's statements");
    require("take it back out", mt_del(m, E("=", E("myfunc"), body)));
    mt_close(m);
    return 0;
}
