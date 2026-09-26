/* Purpose: unification in C, with no engine call. mt_unify() binds variables
 *   on either side, mt_substitute() applies the bindings to a template, and a
 *   repeated variable demands equal fields.
 * Guarantees: the template instantiates, and (pair $x $x) refuses (pair 1 2)
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

/* Whether an atom is want up to renaming variables; takes both, and shows
   them when they differ. */
static inline bool atom_is(mt_atom *got, mt_atom *want)
{
    bool holds = got && want && mt_alpha_eq(got, want);
    if (!holds) fprintf(stderr, "  got %s\n  want %s\n", got ? mt_show(got) : "nothing",
                        want ? mt_show(want) : "nothing");
    mt_drop(got);
    mt_drop(want);
    return holds;
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    mt_atom *pattern = E("Parent", V("parent"), V("child"));
    mt_atom *fact = E("Parent", "Tom", "Bob");
    mt_bindings *bindings = mt_unify(pattern, fact);
    require("the fact unifies", bindings != NULL);
    mt_atom *template = E("Cares", V("parent"), V("child"));
    assert(atom_is(mt_substitute(template, bindings), E("Cares", "Tom", "Bob"))
           && "the bindings fill the template");
    mt_drop(template);
    mt_bindings_free(bindings);
    mt_drop(pattern);
    mt_drop(fact);

    mt_atom *diagonal = E("pair", V("x"), V("x"));
    mt_atom *unequal = E("pair", 1, 2);
    mt_clear();
    assert(mt_unify(diagonal, unequal) == NULL && mt_ok()
           && "a repeated variable refuses unequal fields, without an error");
    mt_drop(diagonal);
    mt_drop(unequal);

    /* The engine's own unify agrees. */
    assert(answers_are(mt_eval(m, E("unify", E("pair", V("x"), V("x")), E("pair", 1, 2), "same", "different")), E("different"))
           && "and so does the engine");
    mt_close(m);
    return 0;
}
