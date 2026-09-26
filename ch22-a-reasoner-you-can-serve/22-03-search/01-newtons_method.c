/* Purpose: a memoized recursive function under a configured cache. The
 *   energy of a point after n halving steps is one body over the C_ and
 *   T_ operators: with C's it is the C function energy(), and with the atom
 *   builders it is the equation the program adds after memoizing the name,
 *   so each value the engine answers is the one the same body computes in C.
 * Guarantees: both claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>

/* A door the program needs before it can go on: on refusal, say why and stop. */
#define require(what, ok) \
    ((ok) ? (void)0 : (fprintf(stderr, "%s: %s\n", (what), mt_errmsg()), exit(EXIT_FAILURE)))

/* Whether a list holds exactly the children of want, in order, each equal
   up to renaming variables; takes both, and shows them when they differ. */
static inline bool list_is(mt_list got, mt_atom *want)
{
    bool holds = mt_ok() && want && got.len == mt_len(want);
    for (size_t i = 0; holds && i < got.len; i++)
        holds = mt_alpha_eq(got.items[i], mt_at(want, i));
    if (!holds) {
        fprintf(stderr, "  got");
        for (size_t i = 0; i < got.len; i++) fprintf(stderr, " %s", mt_show(got.items[i]));
        fprintf(stderr, "\n  want %s\n", want ? mt_show(want) : "nothing");
    }
    mt_list_free(got);
    mt_drop(want);
    return holds;
}

/* Whether a query answers exactly the children of want; takes both. */
static inline bool answers_are(mt_answers *answers, mt_atom *want)
{
    return list_is(mt_all(answers), want);
}

/* Each MeTTa operator twice, for a body written once over its operators:
   C_X computes in C, and T_X builds the atom (X a b). */
#define C_IF(c, t, e) ((c) ? (t) : (e))
#define T_IF(c, t, e) mt_expr("if", c, t, e)
#define C_LE(a, b) ((a) <= (b))
#define T_LE(a, b) mt_expr("<=", a, b)
#define C_ADD(a, b) ((a) + (b))
#define T_ADD(a, b) mt_expr("+", a, b)
#define C_SUB(a, b) ((a) - (b))
#define T_SUB(a, b) mt_expr("-", a, b)
#define C_MUL(a, b) ((a) * (b))
#define T_MUL(a, b) mt_expr("*", a, b)

/* Two identical recursive calls, which is what memoizing pays for. */
#define ENERGY(IF, LE, ADD, MUL, SUB, SELF, x, n)                                                          \
    IF(LE(n, 0), MUL(x, x), ADD(SELF(ADD(MUL(0.5, x), 0.4), SUB(n, 1)), SELF(ADD(MUL(0.5, x), 0.4), SUB(n, 1))))
#define T_ENERGY(x, n) mt_expr("energy", x, n)

/* Time: 2^n calls; the engine's memo makes it n. */
static double energy(double x, int64_t n) { return ENERGY(C_IF, C_LE, C_ADD, C_MUL, C_SUB, energy, x, n); }

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
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
    for (size_t i = 0; i < 2; i++) assert(answers_are(mt_eval(m, T_ENERGY(points[i].x, points[i].n)), E(energy(points[i].x, points[i].n))) && "energy");
    mt_close(m);
    return 0;
}
