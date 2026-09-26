/* Purpose: the engine lives inside a program that was already running. Atoms
 *   are C memory: one is built before the engine opens, an answer is kept
 *   after it closes, and both stay readable.
 * Guarantees: a pre-boot atom takes part in an evaluation and the answer
 *   outlives mt_close() [tested 2026-09-27T00:35:58+10:00:
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

int main(void)
{
    int64_t application_total = 10;
    mt_atom *input = N(32);                 /* no engine is running yet */
    assert(mt_int(input) == 32 && "an atom needs no engine");

    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    mt_atom *answer = mt_one(mt_eval(m, E("+", application_total, input)));
    assert(mt_int(answer) == 42 && "the application's value takes part");
    mt_close(m);
    require("close the engine", mt_ok());

    assert(mt_int(answer) == 42 && "the answer outlives the engine");
    mt_drop(answer);
    return 0;
}
