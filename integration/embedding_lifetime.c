/* Purpose: the engine lives inside a program that was already running. Atoms
 *   are C memory: one is built before the engine opens, an answer is kept
 *   after it closes, and both stay readable.
 * Guarantees: a pre-boot atom takes part in an evaluation and the answer
 *   outlives mt_close() [tested: make check; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    int64_t application_total = 10;
    mt_atom *input = N(32);                 /* no engine is running yet */
    check_int("an atom needs no engine", mt_int(input), 32);

    metta *m = open_engine();
    mt_atom *answer = mt_one(mt_eval(m, E("+", application_total, input)));
    check_int("the application's value takes part", mt_int(answer), 42);
    mt_close(m);
    require("close the engine", mt_ok());

    check_int("the answer outlives the engine", mt_int(answer), 42);
    mt_drop(answer);
    return done(NULL);
}
