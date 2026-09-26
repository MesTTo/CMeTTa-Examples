/* Purpose: a condition with three answers. if-nondet and case-nondet test a
 *   superposition, so each answer takes its own arm; C maps the same array
 *   of booleans through ?: for the answers it expects.
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

static const bool CONDITIONS[] = { true, false, true };
enum { N_CONDITIONS = sizeof CONDITIONS / sizeof *CONDITIONS };

static mt_atom *conditions(void)
{
    mt_atom *kids[N_CONDITIONS];
    for (size_t i = 0; i < N_CONDITIONS; i++) kids[i] = B(CONDITIONS[i]);
    return mt_exprv(N_CONDITIONS, kids);
}

static void expected(mt_atom **want)
{
    for (size_t i = 0; i < N_CONDITIONS; i++) want[i] = S(CONDITIONS[i] ? "a" : "b");
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("if-nondet", mt_add(m, E("=", E("if-nondet", V("y")), E("if", E("superpose", V("y")), "a", "b"))));
    require("case-nondet", mt_add(m, E("=", E("case-nondet", V("y")),
                                      E("case", E("superpose", V("y")), E(E(B(true), "a"), E(B(false), "b"))))));

    mt_atom *want[N_CONDITIONS];
    expected(want);
    assert(list_is(mt_all(mt_eval(m, E("if-nondet", conditions()))), mt_exprv(N_CONDITIONS, want)) && "if takes an arm per answer");
    expected(want);
    assert(list_is(mt_all(mt_eval(m, E("case-nondet", conditions()))), mt_exprv(N_CONDITIONS, want)) && "so does case");
    mt_close(m);
    return 0;
}
