/* Purpose: evaluation from lib_he's side. double is one body over the C_ and
 *   T_ operators, built for the engine and compiled for C; eval, evalc in
 *   &self and chain each answer the sum or product C computes, and
 *   for-each-in-atom maps println! over C's six items, answering the true
 *   println! answers once per item.
 * Guarantees: all four claims of the original hold
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
#define C_ADD(a, b) ((a) + (b))
#define T_ADD(a, b) mt_expr("+", a, b)
#define C_MUL(a, b) ((a) * (b))
#define T_MUL(a, b) mt_expr("*", a, b)

#define DOUBLE(ADD, x) ADD(x, x)

static const int64_t items[] = { 1, 3, 5, 62, 2, 5 };
#define ITEMS (sizeof items / sizeof *items)

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_he", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_he")))));
    require("double", mt_add(m, E("=", E("double", V("x")), DOUBLE(T_ADD, V("x")))));
    assert(mt_one_int(mt_eval(m, E("eval", E("double", 5)))) == DOUBLE(C_ADD, 5) && "eval of a call");
    assert(mt_one_int(mt_eval(m, E("evalc", T_ADD(5, 5), mt_spaceref("&self")))) == C_ADD(5, 5) && "evalc in &self");
    assert(mt_one_int(mt_eval(m, E("chain", T_ADD(2, 3), V("x"), T_MUL(V("x"), 2)))) == C_MUL(C_ADD(2, 3), 2)
           && "chain binds and continues");

    mt_atom *list[ITEMS], *printed[ITEMS];
    for (size_t i = 0; i < ITEMS; i++) list[i] = N(items[i]), printed[i] = B(true);
    assert(answers_are(mt_eval(m, E("for-each-in-atom", mt_exprv(ITEMS, list), "println!")), E(mt_exprv(ITEMS, printed)))
           && "println! answers true once per item");
    mt_close(m);
    return 0;
}
