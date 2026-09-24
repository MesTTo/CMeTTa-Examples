/* Purpose: the exponential fib, tabled. FIB is chapter 7's body, shared
 *   through its fib.h: with lowering.h's C operators it is the recursive C
 *   function fib(), and with MeTTa's tokens the equation mt_lower installs,
 *   which tabled then instruments, so the engine reuses each (fib n) it has
 *   answered and asks each once. It must answer what C's exponential
 *   recursion computes. Declared after the definition, because tabling
 *   refuses a name that is not a function yet.
 * Guarantees: the original's claim holds [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"
#include "../../ch07-control-flow/07-05-recursion/fib.h"

int main(void)
{
    metta *m = open_engine();
    require("import lib_tabling", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_tabling")))));
    require("fib", install_fib(m));
    require("table fib", mt_one_truth(mt_eval(m, E("tabled", E("fib", V("N"))))));
    check_answers("(fib 30) from its table", mt_eval(m, E("fib", 30)), fib(30));
    return done(m);
}
