/* Purpose: the exponential fib, written once and run in both languages.
 *   fib.h's FIB expands to a recursive C function and to the equation
 *   install_fib adds; the engine runs it under a raised branch budget, a
 *   pragma scoped to the one evaluation, and must answer what C computed.
 * Guarantees: the original's claim holds [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include "fib.h"

int main(void)
{
    metta *m = open_engine();
    require("fib", install_fib(m));
    check_answers("(fib 30) under a raised budget",
                  mt_eval(m, E("with-pragma!", E(E("max-stack-depth", 100000000)), E("fib", 30))), fib(30));
    return done(m);
}
