/* Purpose: an arrow whose last parameter is a segment accepts every arity.
 *   do2's (-> (:seg Bool) (->)) evaluates each argument, printing as it
 *   goes, and answers unit whatever the count; undeclared-do binds its whole
 *   run too, which C builds as the list of the arguments it passed.
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
    mt_atom *arrow = E("->", E(":seg", "Bool"), E("->"));
    require("(: do2 (-> (:seg Bool) (->)))", mt_add(m, E(":", "do2", mt_keep(arrow))));
    require("(= (do2 (:seg $args)) ())", mt_add(m, E("=", E("do2", E(":seg", V("args"))), mt_unit())));
    const char *words[] = { "one", "two", "three" };
    mt_atom *runs[4], *units[4];
    for (size_t n = 0; n < 4; n++) {
        mt_atom *call[4] = { S("do2") };
        for (size_t i = 0; i < n; i++) call[1 + i] = E("println!", words[i]);
        runs[n] = mt_exprv(1 + n, call);
        units[n] = mt_unit();
    }
    assert(answers_are(mt_eval(m, mt_exprv(4, runs)), E(mt_exprv(4, units))) && "every arity answers unit");
    assert(answers_are(mt_eval(m, E("get-type", "do2")), E(arrow)) && "the declared arrow");
    require("(= (undeclared-do (:seg $args)) (got $args))", mt_add(m, E("=", E("undeclared-do", E(":seg", V("args"))), E("got", V("args")))));
    assert(answers_are(mt_eval(m, E("undeclared-do", "a", "b")), E(E("got", E("a", "b")))) && "an undeclared segment binds the run");
    assert(answers_are(mt_eval(m, E("undeclared-do")), E(E("got", mt_unit()))) && "an empty run");
    mt_close(m);
    return 0;
}
