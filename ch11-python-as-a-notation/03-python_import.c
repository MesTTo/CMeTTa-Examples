/* Purpose: a Python file imported as a module of functions. C imports the
 *   original's own fixture and holds each function's answer against its C
 *   counterpart: greet's sentence through snprintf and add's sum through +.
 * Guarantees: both claims of the original hold
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
    require("import the fixture", mt_one_truth(mt_eval(m, E("import!", "&self", T("examples/ch11-python-as-a-notation/_fixtures/python_import_file.py")))));
    const char name[] = "MeTTa User";
    char greeting[64];
    int n = snprintf(greeting, sizeof greeting, "Hello, %s from Python!", name);
    require("the greeting fits", n > 0 && (size_t)n < sizeof greeting);
    assert(answers_are(mt_eval(m, E("repr", E("py-call", E("python_import_file.greet", T(name))))), E(T(greeting))) && "greet");
    assert(answers_are(mt_eval(m, E("py-call", E("python_import_file.add", 10, 20))), E(N(10 + 20))) && "add");
    mt_close(m);
    return 0;
}
