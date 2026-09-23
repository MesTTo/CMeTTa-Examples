/* Purpose: reject false and absent checks under NDEBUG.
 * Guarantees: both invocations fail [tested: make check-helpers; commit=WORKTREE].
 * Open Obligations: None.
 */
#include "common.h"
int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "false") == 0) check("planted false claim", false);
    return done(NULL, "unchecked");
}
