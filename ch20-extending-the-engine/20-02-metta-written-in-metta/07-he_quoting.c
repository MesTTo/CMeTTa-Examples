/* Purpose: quoting from lib_he's side. quote and eval are rows of
 *   control_forms.h: quote hands (+ 1 2) over as written, and eval answers
 *   C's sum; unquote undoes a quote, so it answers the sum too. repr spells
 *   (unquote 42) as C writes an expression of a head and a number, and
 *   noreduce-eq compares its arguments as they were written, which is C's
 *   structural equality on the atoms it built.
 * Guarantees: all six claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include "_fixtures/control_forms.h"
#include <inttypes.h>

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

static mt_atom *sum(void) { return T_ADD(1, 2); }

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_he", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_he")))));
    static const char *const forms[] = { "quote", "eval" };
    for (size_t i = 0; i < sizeof forms / sizeof *forms; i++)
        assert(answers_are(mt_eval(m, E(forms[i], sum())), E(answered(control_form_named(forms[i]), sum(), N(C_ADD(1, 2)))))
               && "each form answers as its row says");
    assert(mt_one_int(mt_eval(m, E("unquote", E("quote", sum())))) == C_ADD(1, 2) && "unquote undoes a quote");

    const int64_t unquoted = 42;
    char written[32];
    snprintf(written, sizeof written, "(%s %" PRId64 ")", "unquote", unquoted);
    assert(answers_are(mt_eval(m, E("repr", E("unquote", unquoted))), E(T(written))) && "repr writes the term");

    mt_atom *same = sum(), *three = N(C_ADD(1, 2));
    const mt_atom *compared[] = { same, three };
    for (size_t i = 0; i < 2; i++) {
        mt_atom *left = sum();
        assert(answers_are(mt_eval(m, E("noreduce-eq", mt_keep(left), mt_keep(compared[i]))), E(B(mt_eq(left, compared[i]))))
               && "noreduce-eq compares the atoms as written");
        mt_drop(left);
    }
    mt_drop(same), mt_drop(three);
    mt_close(m);
    return 0;
}
