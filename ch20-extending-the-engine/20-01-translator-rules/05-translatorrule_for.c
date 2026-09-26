/* Purpose: a for loop the language did not have, added as a translator
 *   rule. for is an equation whose body is the let over a superposition it
 *   expands into, built as the term it is; once it is a rule, a definition
 *   written with it compiles to that expansion. myfun's body is CLASSIFY,
 *   one body over the C_ and T_ operators: over their atom builders it is the
 *   loop's body in MeTTa, and over C's operators it is the loop body of the C for loop that
 *   says what the engine must answer, (odd 3) then (even 4).
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
#define C_MOD(a, b) floor_mod(a, b)
#define T_MOD(a, b) mt_expr("%", a, b)

#define CLASSIFY(IF, EQ, MOD, EVEN, ODD, x) IF(EQ(MOD(x, 2), 0), EVEN(x), ODD(x))
#define T_EVEN(x) E("even", x)
#define T_ODD(x) E("odd", x)

static mt_atom *even(int64_t x) { return E("even", x); }
static mt_atom *odd(int64_t x) { return E("odd", x); }

static const int64_t items[] = { 3, 4 };
#define ITEMS (sizeof items / sizeof *items)

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("(: for (-> Atom Atom Atom %Undefined%))", mt_add(m, E(":", "for", E("->", "Atom", "Atom", "Atom", "%Undefined%"))));
    require("for's expansion",
            mt_add(m, E("=", E("for", V("var"), V("collection"), V("body")),
                        E("noeval", E("let", V("var"), E("superpose", V("collection")), V("body"))))));
    require("for is a rule", mt_one_truth(mt_eval(m, E("add-translator-rule!", "for"))));
    require("myfun loops with it", mt_add(m, E("=", E("myfun", V("L")),
                                              E("for", V("x"), V("L"), CLASSIFY(T_IF, T_EQ, T_MOD, T_EVEN, T_ODD, V("x"))))));

    mt_atom *list[ITEMS], *want[ITEMS];
    for (size_t i = 0; i < ITEMS; i++) {
        list[i] = N(items[i]);
        want[i] = CLASSIFY(C_IF, C_EQ, C_MOD, even, odd, items[i]);
    }
    assert(answers_are(mt_eval(m, E("myfun", mt_exprv(ITEMS, list))), mt_exprv(ITEMS, want)) && "each item classified in order");
    mt_close(m);
    return 0;
}
