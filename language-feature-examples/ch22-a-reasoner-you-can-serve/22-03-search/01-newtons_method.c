/* Purpose: a memoized recursive function under a configured cache. The
 *   energy of a point after n halving steps is one body over lowering.h's
 *   operators: with C's it is the C function energy(), and with the atom
 *   builders it is the equation the program adds after memoizing the name,
 *   so each value the engine answers is the one the same body computes in C.
 * Guarantees: both claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"
#include "lowering.h"

/* Two identical recursive calls, which is what memoizing pays for. */
#define ENERGY(IF, LE, ADD, MUL, SUB, SELF, x, n)                                                          \
    IF(LE(n, 0), MUL(x, x), ADD(SELF(ADD(MUL(0.5, x), 0.4), SUB(n, 1)), SELF(ADD(MUL(0.5, x), 0.4), SUB(n, 1))))
#define T_ENERGY(x, n) mt_expr("energy", x, n)

/* Time: 2^n calls; the engine's memo makes it n. */
static double energy(double x, int64_t n) { return ENERGY(C_IF, C_LE, C_ADD, C_MUL, C_SUB, energy, x, n); }

int main(void)
{
    metta *m = open_engine();
    require("lib_memo", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_memo")))));
    /* memoize refuses a name with no equation, so the equation goes in
       first, as the original's file adds its equations before its commands
       run. */
    require("the equation", mt_add(m, E("=", T_ENERGY(V("x"), V("n")), ENERGY(T_IF, T_LE, T_ADD, T_MUL, T_SUB, T_ENERGY, V("x"), V("n")))));
    require("configure the memo", mt_one_truth(mt_eval(m, E("config-memoize", E("strategy", "wtinylfu"), E("unique-limit", 100)))));
    require("memoize energy", mt_one_truth(mt_eval(m, E("memoize", "energy"))));

    static const struct {
        double x;
        int64_t n;
    } points[] = { { 2.0, 0 }, { 2.0, 1 } };
    for (size_t i = 0; i < 2; i++) check_answers("energy", mt_eval(m, T_ENERGY(points[i].x, points[i].n)), energy(points[i].x, points[i].n));
    return done(m);
}
