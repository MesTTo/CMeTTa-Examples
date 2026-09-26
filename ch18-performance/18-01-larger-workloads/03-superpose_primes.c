/* Purpose: trial division, written once. FIND_DIVISOR is a macro body over
 *   its operators and its own name: with the C_ operators it is the
 *   C function find_divisor(), whose self-call is a tail call C runs as a
 *   loop, and with its atom builders it is the equation mt_add() installs, as
 *   PRIME is prime?'s. The engine runs the four searches as one tuple under
 *   the original's branch budget, a pragma scoped to that one evaluation, and
 *   must answer what C computed for each candidate.
 * Guarantees: the original's claim holds [tested 2026-09-27T00:35:58+10:00:
 *   make -C extensions/cmetta corpus-check].
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

/* The remainder whose sign is the divisor's, which is what MeTTa's %
   answers, where C's % takes the dividend's sign. */
static inline int64_t floor_mod(int64_t a, int64_t b)
{
    int64_t r = a % b;
    return r != 0 && (r < 0) != (b < 0) ? r + b : r;
}

/* Each MeTTa operator twice, for a body written once over its operators:
   C_X computes in C, and T_X builds the atom (X a b). */
#define C_IF(c, t, e) ((c) ? (t) : (e))
#define T_IF(c, t, e) mt_expr("if", c, t, e)
#define C_EQ(a, b) ((a) == (b))
#define T_EQ(a, b) mt_expr("==", a, b)
#define C_GT(a, b) ((a) > (b))
#define T_GT(a, b) mt_expr(">", a, b)
#define C_ADD(a, b) ((a) + (b))
#define T_ADD(a, b) mt_expr("+", a, b)
#define C_MUL(a, b) ((a) * (b))
#define T_MUL(a, b) mt_expr("*", a, b)
#define C_MOD(a, b) floor_mod(a, b)
#define T_MOD(a, b) mt_expr("%", a, b)

#define FIND_DIVISOR(IF, GT, MUL, EQ, MOD, ADD, SELF, n, d) \
    IF(GT(MUL(d, d), n), n, IF(EQ(0, MOD(n, d)), d, SELF(n, ADD(d, 1))))
#define PRIME(EQ, FIND, n) EQ(n, FIND(n, 2))
#define T_FIND_DIVISOR(n, d) E("find-divisor", n, d)

static int64_t find_divisor(int64_t n, int64_t d) { return FIND_DIVISOR(C_IF, C_GT, C_MUL, C_EQ, C_MOD, C_ADD, find_divisor, n, d); }
static bool prime(int64_t n) { return PRIME(C_EQ, find_divisor, n); }

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("find-divisor", mt_add(m, E("=", T_FIND_DIVISOR(V("n"), V("test-divisor")),
                                       FIND_DIVISOR(T_IF, T_GT, T_MUL, T_EQ, T_MOD, T_ADD, T_FIND_DIVISOR, V("n"), V("test-divisor")))));
    require("prime?", mt_add(m, E("=", E("prime?", V("n")), PRIME(T_EQ, T_FIND_DIVISOR, V("n")))));

    static const int64_t candidates[] = { 53537257, 53781811, 54218443, 54734431 };
    mt_atom *searches[4], *verdicts[4];
    for (size_t i = 0; i < 4; i++) {
        searches[i] = E("prime?", candidates[i]);
        verdicts[i] = B(prime(candidates[i]));
    }
    assert(answers_are(mt_eval(m, E("with-pragma!", E(E("max-stack-depth", 1000000)), mt_exprv(4, searches))), E(mt_exprv(4, verdicts)))
           && "four divisor searches under one branch budget");
    mt_close(m);
    return 0;
}
