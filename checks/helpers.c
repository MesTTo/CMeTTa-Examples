/* Purpose: reject false and absent checks under NDEBUG.
 * Guarantees: both invocations fail [tested: make check-helpers; commit=6022c3f48b6dc64752c6e49cfe9d985c7ac7a4e9].
 * Open Obligations: None.
 */
#include "common.h"
int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "false") == 0) check("planted false claim", false);
    return done(NULL, "unchecked");
}
