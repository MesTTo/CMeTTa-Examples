/* Purpose: case takes the first branch that matches, and a variable pattern
 *   matches anything, so it is C's switch with a default. casetest is
 *   lowered from C tokens and the C function beside it is the switch; the
 *   engine's answer must be the switch's.
 * Guarantees: the original's claim holds [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

static int64_t casetest(int64_t x)
{
    switch (x) {
    case 4: return 42;
    default: return 44;          /* ($otherpattern 44); ($otherother $45) is never reached */
    }
}

int main(void)
{
    metta *m = open_engine();
    require("casetest", mt_lower(m, (casetest $x), (case $x ((4 42) ($otherpattern 44) ($otherother $45)))));
    check_answers("(casetest 5)", mt_eval(m, E("casetest", 5)), casetest(5));
    return done(m);
}
