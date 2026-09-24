/* Purpose: the exponential fib, tabled. FIB is one body over its operators
 *   and its own name: with lowering.h's C operators it is the recursive C
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
#include "lowering.h"

#define FIB(IF, LT, ADD, SUB, SELF, n) IF(LT(n, 2), n, ADD(SELF(SUB(n, 1)), SELF(SUB(n, 2))))
#define M_FIB(n) (fib n)

static int64_t fib(int64_t n) { return FIB(C_IF, C_LT, C_ADD, C_SUB, fib, n); }

int main(void)
{
    metta *m = open_engine();
    require("import lib_tabling", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_tabling")))));
    require("fib", mt_lower(m, (fib $N), FIB(M_IF, M_LT, M_ADD, M_SUB, M_FIB, $N)));
    require("table fib", mt_one_truth(mt_eval(m, E("tabled", E("fib", V("N"))))));
    check_answers("(fib 30) from its table", mt_eval(m, E("fib", 30)), fib(30));
    return done(m);
}
