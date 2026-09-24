/* Purpose: prove the claim helpers refuse, with NDEBUG defined, a false claim
 *   and a program that claimed nothing.
 * Guarantees: both invocations exit nonzero with a FAIL line
 *   [tested: make check-helpers; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#include "common.h"

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "false") == 0) check("a planted false claim", false);
    return done(NULL);
}
