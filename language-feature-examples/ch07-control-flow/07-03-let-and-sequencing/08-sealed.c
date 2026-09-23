/* Purpose: freshening an atom's variables. sealed answers an atom whose
 *   variables are fresh except the ones it is told to keep; C holds each
 *   returned atom and runs it with mt_eval when it wants it run. Where a
 *   claim is that two variables are distinct, the engine collapses them into
 *   one answer, so the alpha comparison's bijection sees both at once.
 * Guarantees: all ten claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

static mt_atom *sealed(metta *m, mt_atom *keep, mt_atom *body)
{
    return mt_one(mt_eval(m, E("sealed", keep, body)));
}

int main(void)
{
    metta *m = open_engine();
    check_answers("a sealed let runs later", mt_eval(m, sealed(m, mt_unit(), E("let", V("x"), 2, V("x")))), 2);
    check_answers("an unbound one too", mt_eval(m, sealed(m, mt_unit(), E("let", V("y"), 5, V("y")))), 5);
    check_answers("a kept variable keeps its binding",
                  mt_eval(m, E("let", V("z"), 7, E("sealed", E(V("z")), E(V("z"), V("w"))))), E(7, V("unbound")));
    check_answers("a ground atom has nothing to rename", mt_eval(m, E("sealed", mt_unit(), 42)), 42);

    mt_atom *outer = sealed(m, mt_unit(), E("let", V("n"), 2, E("sealed", mt_unit(), E("let", V("n"), 3, V("n")))));
    mt_atom *step = mt_one(mt_eval(m, outer));
    check_answers("each eval consumes one layer", mt_eval(m, step), 3);

    require("mk-tagger", mt_add(m, E("=", E("mk-tagger"),
                                     E("|->", E(V("item")), E("sealed", E(V("item")), E("tagged", V("item"), V("fresh")))))));
    check_answers("each application's free variable is its own",
                  mt_eval(m, E("collapse", E("let", V("f"), E("mk-tagger"),
                                             E("superpose", E(E(V("f"), 1), E(V("f"), 2)))))),
                  E(E("tagged", 1, V("a")), E("tagged", 2, V("b"))));
    check_answers("an ignored variable keeps its surrounding binding",
                  mt_eval(m, E("let", V("outer"), 7, E("sealed", E(V("outer")), E("both", V("outer"), V("local"))))),
                  E("both", 7, V("c")));

    for (int i = 0; i < 2; i++)
        require("store a sealed rule", mt_add(m, sealed(m, mt_unit(), E("stored-rule", V("r"), "ok"))));
    check_answers("two stored rules, two variables",
                  mt_eval(m, E("collapse", E("match", "&self", E("stored-rule", V("x"), V("y")), E(V("x"), V("y"))))),
                  E(E(V("p"), "ok"), E(V("q"), "ok")));

    check_answers("the kept $y is bound, $x is fresh",
                  mt_eval(m, E("let", V("x"), 1, E("let", V("y"), 2, E("sealed", E(V("y")), E("pair", V("x"), V("y")))))),
                  E("pair", V("fresh"), 2));
    check_answers("the returned atom is inert until run", mt_eval(m, sealed(m, mt_unit(), E("+", 1, 2))), 3);
    return done(m);
}
