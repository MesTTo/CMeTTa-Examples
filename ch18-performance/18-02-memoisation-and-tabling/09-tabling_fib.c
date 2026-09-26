/* Purpose: the exponential fib, tabled. FIB is chapter 7's body, shared
 *   through its fib.h: with the C_ operators it is the recursive C
 *   function fib(), and with its atom builders the equation install_fib adds,
 *   which tabled then instruments, so the engine reuses each (fib n) it has
 *   answered and asks each once. It must answer what C's exponential
 *   recursion computes. Declared after the definition, because tabling
 *   refuses a name that is not a function yet.
 * Guarantees: the original's claim holds [tested 2026-09-27T00:35:58+10:00:
 *   make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include "../../ch20-extending-the-engine/20-02-metta-written-in-metta/_fixtures/fib.h"

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
    require("import lib_tabling", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_tabling")))));
    require("fib", install_fib(m));
    require("table fib", mt_one_truth(mt_eval(m, E("tabled", E("fib", V("N"))))));
    assert(answers_are(mt_eval(m, E("fib", 30)), E(fib(30))) && "(fib 30) from its table");
    mt_close(m);
    return 0;
}
