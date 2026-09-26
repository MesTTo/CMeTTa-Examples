/* Purpose: recursion through a conditional, written once. FAC is a macro
 *   body over its operators and its own name: expanded with C's operators
 *   and facF it is a recursive C function, and expanded with the atom
 *   builders it is the equation mt_add installs. The engine's (facF 10) must be the C
 *   function's.
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

/* Each MeTTa operator twice, for a body written once over its operators:
   C_X computes in C, and T_X builds the atom (X a b). */
#define C_IF(c, t, e) ((c) ? (t) : (e))
#define T_IF(c, t, e) mt_expr("if", c, t, e)
#define C_EQ(a, b) ((a) == (b))
#define T_EQ(a, b) mt_expr("==", a, b)
#define C_SUB(a, b) ((a) - (b))
#define T_SUB(a, b) mt_expr("-", a, b)
#define C_MUL(a, b) ((a) * (b))
#define T_MUL(a, b) mt_expr("*", a, b)

#define FAC(IF, EQ, MUL, SUB, SELF, n) IF(EQ(n, 0), 1, MUL(n, SELF(SUB(n, 1))))
#define T_FACF(n) E("facF", n)

static int64_t facF(int64_t n) { return FAC(C_IF, C_EQ, C_MUL, C_SUB, facF, n); }

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("facF", mt_add(m, E("=", T_FACF(V("n")), FAC(T_IF, T_EQ, T_MUL, T_SUB, T_FACF, V("n")))));
    assert(answers_are(mt_eval(m, E("facF", 10)), E(facF(10))) && "(facF 10)");
    mt_close(m);
    return 0;
}
