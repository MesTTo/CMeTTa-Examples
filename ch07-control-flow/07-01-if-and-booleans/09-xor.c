/* Purpose: xor inside an equation, written once. CHECK_XOR is a macro body
 *   over its operators: with C's ?:, != on booleans for xor, == and > it is
 *   the C function check_xor, and with the atom builders it is the equation
 *   mt_add installs under the same name, underscore and all. The engine's
 *   answers must be the C function's.
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
#define C_EQ(a, b) ((a) == (b))
#define T_EQ(a, b) mt_expr("==", a, b)
#define C_GT(a, b) ((a) > (b))
#define T_GT(a, b) mt_expr(">", a, b)
#define C_XOR(a, b) ((a) != (b))
#define T_XOR(a, b) mt_expr("xor", a, b)

#define CHECK_XOR(IF, XOR, EQ, GT, s, d) IF(XOR(EQ(s, d), GT(s, d)), 42, 0)

static int64_t check_xor(int64_t s, int64_t d) { return CHECK_XOR(C_IF, C_XOR, C_EQ, C_GT, s, d); }

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("check_xor", mt_add(m, E("=", E("check_xor", V("source"), V("destination")),
                                    CHECK_XOR(T_IF, T_XOR, T_EQ, T_GT, V("source"), V("destination")))));
    assert(answers_are(mt_eval(m, E("check_xor", 2, 2)), E(check_xor(2, 2))) && "(check_xor 2 2)");
    assert(answers_are(mt_eval(m, E("check_xor", 4, 2)), E(check_xor(4, 2))) && "(check_xor 4 2)");
    mt_close(m);
    return 0;
}
