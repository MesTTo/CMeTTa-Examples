/* Purpose: a branch is evaluated. With a space operand, unify's then and
 *   else branches are reduced before they answer, so an (Error ...) built in
 *   a branch comes back as that error, a nested unify in the else branch
 *   runs, and (+ 1 2) in a branch answers 3; this is how a checker declares a
 *   conflict as data.
 * Guarantees: all five claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

static mt_atom *constant(const char *name) { return E("Constant", name, E("Type", T("$c"))); }
static mt_atom *variable(const char *name) { return E("Var", name, 0, E("Type", T("$v"))); }

/* (unify &self <fact> <then> <else>) */
static mt_atom *declared(mt_atom *fact, mt_atom *then, mt_atom *otherwise)
{
    return E("unify", mt_spaceref("&self"), fact, then, otherwise);
}

int main(void)
{
    metta *m = open_engine();
    require("import lib_he", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_he")))));
    require("declare the constant wff", mt_add(m, constant("wff")));
    require("declare the variable x", mt_add(m, variable("x")));

    check_answers("the then branch answers its error",
                  mt_eval(m, declared(constant("wff"), E("Error", E("Constant", "wff"), T("already declared")),
                                      mt_unit())),
                  E("Error", E("Constant", "wff"), T("already declared")));
    check_answers("the else branch runs its nested unify through to ()",
                  mt_eval(m, declared(constant("y"), E("Error", E("Constant", "y"), T("already declared")),
                                      declared(variable("y"), E("Error", E("Var", "y"), T("active variable conflict")),
                                               mt_unit()))),
                  mt_unit());
    check_answers("and to the conflict when it exists",
                  mt_eval(m, declared(constant("x"), E("Error", E("Constant", "x"), T("already declared")),
                                      declared(variable("x"), E("Error", E("Var", "x"), T("active variable conflict")),
                                               mt_unit()))),
                  E("Error", E("Var", "x"), T("active variable conflict")));
    check_int("arithmetic in the then branch is reduced",
              mt_one_int(mt_eval(m, declared(constant("wff"), E("+", 1, 2), N(0)))), 3);
    check_int("and in the else branch",
              mt_one_int(mt_eval(m, declared(constant("NOSUCH"), N(0), E("+", 10, 20)))), 30);
    return done(m);
}
