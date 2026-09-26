/* Purpose: a destructuring binding. f's let* unifies ($f1 $c1 3) with
 *   (1 2 $d1), binding a variable on each side. C does the same in pure C:
 *   mt_unify the pattern with the value and mt_substitute the result into
 *   the body, and the engine's (f) must answer what C computed.
 * Guarantees: the original's claim holds [tested 2026-09-27T00:35:58+10:00:
 *   make -C extensions/cmetta corpus-check].
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
    require("f", mt_add(m, E("=", E("f"), E("let*", E(E(E(V("f1"), V("c1"), 3), E(1, 2, V("d1")))), E(V("f1"), V("c1"), V("d1"))))));

    mt_atom *pattern = E(V("f1"), V("c1"), 3), *value = E(1, 2, V("d1")), *body = E(V("f1"), V("c1"), V("d1"));
    mt_bindings *theta = mt_unify(pattern, value);
    require("the pattern unifies with the value", theta != NULL);
    mt_atom *computed = mt_substitute(body, theta);
    mt_bindings_free(theta);
    mt_drop(pattern);
    mt_drop(value);
    mt_drop(body);
    assert(answers_are(mt_eval(m, E("f")), E(computed)) && "(f) is the substituted body");
    mt_close(m);
    return 0;
}
