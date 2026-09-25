/* Purpose: trial division, written once. FIND_DIVISOR is a macro body over
 *   its operators and its own name: with lowering.h's C operators it is the
 *   C function find_divisor(), whose self-call is a tail call C runs as a
 *   loop, and with its atom builders it is the equation mt_add() installs, as
 *   PRIME is prime?'s. The engine runs the four searches as one tuple under
 *   the original's branch budget, a pragma scoped to that one evaluation, and
 *   must answer what C computed for each candidate.
 * Guarantees: the original's claim holds [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include "lowering.h"

#define FIND_DIVISOR(IF, GT, MUL, EQ, MOD, ADD, SELF, n, d) \
    IF(GT(MUL(d, d), n), n, IF(EQ(0, MOD(n, d)), d, SELF(n, ADD(d, 1))))
#define PRIME(EQ, FIND, n) EQ(n, FIND(n, 2))
#define T_FIND_DIVISOR(n, d) E("find-divisor", n, d)

static int64_t find_divisor(int64_t n, int64_t d) { return FIND_DIVISOR(C_IF, C_GT, C_MUL, C_EQ, C_MOD, C_ADD, find_divisor, n, d); }
static bool prime(int64_t n) { return PRIME(C_EQ, find_divisor, n); }

int main(void)
{
    metta *m = open_engine();
    require("find-divisor", mt_add(m, E("=", T_FIND_DIVISOR(V("n"), V("test-divisor")),
                                       FIND_DIVISOR(T_IF, T_GT, T_MUL, T_EQ, T_MOD, T_ADD, T_FIND_DIVISOR, V("n"), V("test-divisor")))));
    require("prime?", mt_add(m, E("=", E("prime?", V("n")), PRIME(T_EQ, T_FIND_DIVISOR, V("n")))));

    static const int64_t candidates[] = { 53537257, 53781811, 54218443, 54734431 };
    mt_atom *searches[4], *verdicts[4];
    for (size_t i = 0; i < 4; i++) {
        searches[i] = E("prime?", candidates[i]);
        verdicts[i] = B(prime(candidates[i]));
    }
    check_answers("four divisor searches under one branch budget",
                  mt_eval(m, E("with-pragma!", E(E("max-stack-depth", 1000000)), mt_exprv(4, searches))),
                  mt_exprv(4, verdicts));
    return done(m);
}
