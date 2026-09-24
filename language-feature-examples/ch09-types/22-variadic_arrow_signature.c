/* Purpose: an arrow whose last parameter is a segment accepts every arity.
 *   do2's (-> (:seg Bool) (->)) evaluates each argument, printing as it
 *   goes, and answers unit whatever the count; undeclared-do binds its whole
 *   run too, which C builds as the list of the arguments it passed.
 * Guarantees: all four claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
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
    check_answers("every arity answers unit", mt_eval(m, mt_exprv(4, runs)), mt_exprv(4, units));
    check_answers("the declared arrow", mt_eval(m, E("get-type", "do2")), arrow);
    require("(= (undeclared-do (:seg $args)) (got $args))", mt_add(m, E("=", E("undeclared-do", E(":seg", V("args"))), E("got", V("args")))));
    check_answers("an undeclared segment binds the run", mt_eval(m, E("undeclared-do", "a", "b")), E("got", E("a", "b")));
    check_answers("an empty run", mt_eval(m, E("undeclared-do")), E("got", mt_unit()));
    return done(m);
}
