/* Purpose: share always-enabled assertions and engine cleanup across examples.
 * Guarantees: done refuses an unchecked example [tested: make check-helpers; commit=WORKTREE].
 * Open Obligations: None.
 */
#ifndef EXAMPLES_COMMON_H
#define EXAMPLES_COMMON_H
#include <cmetta.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void check(const char *label, bool condition);
void check_atom(const char *label, const mt_atom *actual, const char *expected);
void check_answers(const char *label, mt_answers *answers, const char *expected);
metta *open_engine(void);
int done(metta *runtime, const char *name);
#endif
