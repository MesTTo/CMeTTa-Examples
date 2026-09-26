/* Purpose: a foreign space that holds rules as well as facts. The store is
 *   chapter 19's C array, c_store.h, behind cmetta's provider door with the
 *   rules promise, where the original's store is thirteen lines of Prolog
 *   declaring the same capability. The engine compiles each equation added
 *   there, so a rule in the C store is the same compiled clause a native one
 *   is, and it answers in the space that holds it, asked through (metta goal
 *   type space). Each rule's body is written once over the C_ and
 *   T_ operators, built into the store as an atom and compiled for C, and each answer is
 *   what C computes: fdouble's double, fpick's answers as a set, sorted by
 *   mt_order against C's own, fplain's bare symbol, fnest's arithmetic
 *   evaluated inside out, and ffact's factorial with if taking only its
 *   branch. The space is still a data source, and the one edge C stored is
 *   the one match finds.
 * Guarantees: all six claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include "../../ch19-spaces-backed-by-anything/19-02-a-space-in-c/_fixtures/c_store.h"

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
#define C_GT(a, b) ((a) > (b))
#define T_GT(a, b) mt_expr(">", a, b)
#define C_ADD(a, b) ((a) + (b))
#define T_ADD(a, b) mt_expr("+", a, b)
#define C_SUB(a, b) ((a) - (b))
#define T_SUB(a, b) mt_expr("-", a, b)
#define C_MUL(a, b) ((a) * (b))
#define T_MUL(a, b) mt_expr("*", a, b)

#define FDOUBLE(MUL, x) MUL(2, x)
#define FNEST(ADD, MUL) ADD(1, MUL(2, 3))
#define FFACT(IF, GT, MUL, SUB, SELF, x) IF(GT(x, 0), MUL(x, SELF(SUB(x, 1))), 1)
#define T_FFACT(x) E("ffact", x)

static int64_t ffact(int64_t x) { return FFACT(C_IF, C_GT, C_MUL, C_SUB, ffact, x); }

static c_store rules = C_STORE_INIT;
static const char *const picks[] = { "one", "two" };
#define PICKS (sizeof picks / sizeof *picks)

/* (metta goal %Undefined% &rule_demo): evaluation in the space that holds
   the rule. TAKES goal. */
static mt_answers *in_space(metta *m, mt_atom *goal)
{
    return mt_eval(m, E("metta", goal, "%Undefined%", mt_spaceref("&rule_demo")));
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_import", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_import")))));
    require("the store holds rules", mt_provider_open(m, "&rule_demo", c_store_provider(&rules, true)));
    mt_space *demo = mt_space_open(m, "&rule_demo");
    require("a handle on &rule_demo", demo != NULL);

    require("fdouble", mt_add(demo, E("=", E("fdouble", V("x")), FDOUBLE(T_MUL, V("x")))));
    const int64_t doubled = 21;
    assert(mt_one_int(in_space(m, E("fdouble", doubled))) == FDOUBLE(C_MUL, doubled)
           && "a rule in the foreign space evaluates");

    for (size_t i = 0; i < PICKS; i++) require("an fpick", mt_add(demo, E("=", E("fpick"), picks[i])));
    mt_list got = mt_all(in_space(m, E("fpick")));
    qsort(got.items, got.len, sizeof *got.items, mt_order);
    mt_atom *want[PICKS];
    for (size_t i = 0; i < PICKS; i++) want[i] = S(picks[i]);
    qsort(want, PICKS, sizeof *want, mt_order);
    assert(list_is(got, mt_exprv(PICKS, want)) && "several equations are an answer set");

    require("fplain", mt_add(demo, E("=", E("fplain"), "settled")));
    assert(answers_are(in_space(m, E("fplain")), E("settled")) && "a body that is not a call is the answer");

    require("fnest", mt_add(demo, E("=", E("fnest"), FNEST(T_ADD, T_MUL))));
    assert(mt_one_int(in_space(m, E("fnest"))) == FNEST(C_ADD, C_MUL) && "a nested body is evaluated inside out");

    require("ffact", mt_add(demo, E("=", T_FFACT(V("x")), FFACT(T_IF, T_GT, T_MUL, T_SUB, T_FFACT, V("x")))));
    const int64_t n = 5;
    assert(mt_one_int(in_space(m, E("ffact", n))) == ffact(n) && "a rule recurses, if taking only its branch");

    require("a fact beside the rules", mt_add(demo, E("edge", "a", "b")));
    assert(answers_are(mt_match(demo, E("edge", V("x"), V("y"))), E(E("edge", "a", "b"))) && "and the space is still a data source");

    mt_space_close(demo);
    require("withdraw the store", mt_provider_close(m, "&rule_demo"));
    mt_close(m);
    return 0;
}
