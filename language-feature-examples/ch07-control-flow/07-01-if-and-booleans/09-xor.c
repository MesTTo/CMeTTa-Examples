/* Purpose: xor inside an equation, written once. CHECK_XOR is a macro body
 *   over its operators: with C's ?:, != on booleans for xor, == and > it is
 *   the C function check_xor, and with MeTTa's tokens it is the equation
 *   mt_lower installs under the same name, underscore and all. The engine's
 *   answers must be the C function's.
 * Guarantees: both claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

#define CHECK_XOR(IF, XOR, EQ, GT, s, d) IF(XOR(EQ(s, d), GT(s, d)), 42, 0)
#define C_IF(c, t, e) ((c) ? (t) : (e))
#define C_XOR(a, b) ((a) != (b))
#define C_EQ(a, b) ((a) == (b))
#define C_GT(a, b) ((a) > (b))
#define M_IF(c, t, e) (if c t e)
#define M_XOR(a, b) (xor a b)
#define M_EQ(a, b) (== a b)
#define M_GT(a, b) (> a b)

static int64_t check_xor(int64_t s, int64_t d) { return CHECK_XOR(C_IF, C_XOR, C_EQ, C_GT, s, d); }

int main(void)
{
    metta *m = open_engine();
    require("check_xor", mt_lower(m, (check_xor $source $destination),
                                  CHECK_XOR(M_IF, M_XOR, M_EQ, M_GT, $source, $destination)));
    check_answers("(check_xor 2 2)", mt_eval(m, E("check_xor", 2, 2)), check_xor(2, 2));
    check_answers("(check_xor 4 2)", mt_eval(m, E("check_xor", 4, 2)), check_xor(4, 2));
    return done(m);
}
