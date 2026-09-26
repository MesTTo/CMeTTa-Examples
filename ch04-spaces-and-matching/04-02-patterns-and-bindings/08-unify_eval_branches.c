/* Purpose: a branch is evaluated. With a space operand, unify's then and
 *   else branches are reduced before they answer, so an (Error ...) built in
 *   a branch comes back as that error, a nested unify in the else branch
 *   runs, and (+ 1 2) in a branch answers 3; this is how a checker declares a
 *   conflict as data.
 * Guarantees: all five claims of the original hold
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

static mt_atom *constant(const char *name) { return E("Constant", name, E("Type", T("$c"))); }
static mt_atom *variable(const char *name) { return E("Var", name, 0, E("Type", T("$v"))); }

/* (unify &self <fact> <then> <else>) */
static mt_atom *declared(mt_atom *fact, mt_atom *then, mt_atom *otherwise)
{
    return E("unify", mt_spaceref("&self"), fact, then, otherwise);
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_he", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_he")))));
    require("declare the constant wff", mt_add(m, constant("wff")));
    require("declare the variable x", mt_add(m, variable("x")));

    assert(answers_are(mt_eval(m, declared(constant("wff"), E("Error", E("Constant", "wff"), T("already declared")),
                                           mt_unit())), E(E("Error", E("Constant", "wff"), T("already declared"))))
           && "the then branch answers its error");
    assert(answers_are(mt_eval(m, declared(constant("y"), E("Error", E("Constant", "y"), T("already declared")),
                                           declared(variable("y"), E("Error", E("Var", "y"), T("active variable conflict")),
                                                    mt_unit()))), E(mt_unit()))
           && "the else branch runs its nested unify through to ()");
    assert(answers_are(mt_eval(m, declared(constant("x"), E("Error", E("Constant", "x"), T("already declared")),
                                           declared(variable("x"), E("Error", E("Var", "x"), T("active variable conflict")),
                                                    mt_unit()))), E(E("Error", E("Var", "x"), T("active variable conflict"))))
           && "and to the conflict when it exists");
    assert(mt_one_int(mt_eval(m, declared(constant("wff"), E("+", 1, 2), N(0)))) == 3
           && "arithmetic in the then branch is reduced");
    assert(mt_one_int(mt_eval(m, declared(constant("NOSUCH"), N(0), E("+", 10, 20)))) == 30
           && "and in the else branch");
    mt_close(m);
    return 0;
}
