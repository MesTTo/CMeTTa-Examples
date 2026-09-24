/* Purpose: one implementation under two overload declarations. C declares
 *   both arrows from a table, get-type answers the table in order, and the
 *   single identity equation answers each argument, a number and a text, as
 *   itself.
 * Guarantees: all three claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    const char *types[] = { "Number", "String" };
    mt_atom *arrows[2];
    for (size_t i = 0; i < 2; i++) {
        arrows[i] = E("->", types[i], types[i]);
        require("declare an overload", mt_add(m, E(":", "compiled-identity", mt_keep(arrows[i]))));
    }
    require("(= (compiled-identity $value) $value)", mt_add(m, E("=", E("compiled-identity", V("value")), V("value"))));
    check_answers("both overloads", mt_eval(m, E("collapse", E("get-type", "compiled-identity"))), mt_exprv(2, arrows));
    mt_atom *values[] = { N(7), T("word") };
    for (size_t i = 0; i < 2; i++) {
        check_answers(i ? "a text is itself" : "a number is itself", mt_eval(m, E("compiled-identity", mt_keep(values[i]))), mt_keep(values[i]));
        mt_drop(values[i]);
    }
    return done(m);
}
