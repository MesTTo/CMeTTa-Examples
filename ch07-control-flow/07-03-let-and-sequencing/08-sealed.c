/* Purpose: freshening an atom's variables. sealed answers an atom whose
 *   variables are fresh except the ones it is told to keep; C holds each
 *   returned atom and runs it with mt_eval when it wants it run. Where a
 *   claim is that two variables are distinct, the engine collapses them into
 *   one answer, so the alpha comparison's bijection sees both at once.
 * Guarantees: all ten claims of the original hold
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

static mt_atom *sealed(metta *m, mt_atom *keep, mt_atom *body)
{
    return mt_one(mt_eval(m, E("sealed", keep, body)));
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    assert(answers_are(mt_eval(m, sealed(m, mt_unit(), E("let", V("x"), 2, V("x")))), E(2)) && "a sealed let runs later");
    assert(answers_are(mt_eval(m, sealed(m, mt_unit(), E("let", V("y"), 5, V("y")))), E(5)) && "an unbound one too");
    assert(answers_are(mt_eval(m, E("let", V("z"), 7, E("sealed", E(V("z")), E(V("z"), V("w"))))), E(E(7, V("unbound"))))
           && "a kept variable keeps its binding");
    assert(answers_are(mt_eval(m, E("sealed", mt_unit(), 42)), E(42)) && "a ground atom has nothing to rename");

    mt_atom *outer = sealed(m, mt_unit(), E("let", V("n"), 2, E("sealed", mt_unit(), E("let", V("n"), 3, V("n")))));
    mt_atom *step = mt_one(mt_eval(m, outer));
    assert(answers_are(mt_eval(m, step), E(3)) && "each eval consumes one layer");

    require("mk-tagger", mt_add(m, E("=", E("mk-tagger"),
                                     E("|->", E(V("item")), E("sealed", E(V("item")), E("tagged", V("item"), V("fresh")))))));
    assert(answers_are(mt_eval(m, E("collapse", E("let", V("f"), E("mk-tagger"),
                                                  E("superpose", E(E(V("f"), 1), E(V("f"), 2)))))), E(E(E("tagged", 1, V("a")), E("tagged", 2, V("b")))))
           && "each application's free variable is its own");
    assert(answers_are(mt_eval(m, E("let", V("outer"), 7, E("sealed", E(V("outer")), E("both", V("outer"), V("local"))))), E(E("both", 7, V("c"))))
           && "an ignored variable keeps its surrounding binding");

    for (int i = 0; i < 2; i++)
        require("store a sealed rule", mt_add(m, sealed(m, mt_unit(), E("stored-rule", V("r"), "ok"))));
    assert(answers_are(mt_eval(m, E("collapse", E("match", "&self", E("stored-rule", V("x"), V("y")), E(V("x"), V("y"))))), E(E(E(V("p"), "ok"), E(V("q"), "ok"))))
           && "two stored rules, two variables");

    assert(answers_are(mt_eval(m, E("let", V("x"), 1, E("let", V("y"), 2, E("sealed", E(V("y")), E("pair", V("x"), V("y")))))), E(E("pair", V("fresh"), 2)))
           && "the kept $y is bound, $x is fresh");
    assert(answers_are(mt_eval(m, sealed(m, mt_unit(), E("+", 1, 2))), E(3)) && "the returned atom is inert until run");
    mt_close(m);
    return 0;
}
