/* Purpose: four ways to hand one call over. fib is chapter 7's, installed
 *   from fib.h, and myfunc answers C's constant; each of call, quote, eval
 *   and reduce wraps (fib (myfunc)) in a definition C builds from its row
 *   of control_forms.h. The claims wrap each answer in a symbol of its own,
 *   so what is compared is what came back: C's fib of its constant where
 *   the form reduces, and the call as written, (myfunc) included, where
 *   quote hands it over.
 * Guarantees: all four claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include "_fixtures/fib.h"
#include "_fixtures/control_forms.h"

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

enum { MYFUNC = 5 };

static mt_atom *inner(void) { return T_FIB(E("myfunc")); }

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("fib", install_fib(m));
    require("myfunc", mt_add(m, E("=", E("myfunc"), MYFUNC)));
    for (size_t i = 0; i < CONTROL_FORMS; i++) {
        char definition[32];
        snprintf(definition, sizeof definition, "%s-fib", control_forms[i].name);
        require(definition, mt_add(m, E("=", E(definition), E(control_forms[i].name, inner()))));
    }
    for (size_t i = 0; i < CONTROL_FORMS; i++) {
        char definition[32], label[32];
        snprintf(definition, sizeof definition, "%s-fib", control_forms[i].name);
        snprintf(label, sizeof label, "fib-%s", control_forms[i].name);
        assert(answers_are(mt_eval(m, E(label, E(definition))), E(E(label, answered(&control_forms[i], inner(), N(fib(MYFUNC)))))));
    }
    mt_close(m);
    return 0;
}
