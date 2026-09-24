/* Purpose: quoting from lib_he's side. quote and eval are rows of
 *   control_forms.h: quote hands (+ 1 2) over as written, and eval answers
 *   C's sum; unquote undoes a quote, so it answers the sum too. repr spells
 *   (unquote 42) as C writes an expression of a head and a number, and
 *   noreduce-eq compares its arguments as they were written, which is C's
 *   structural equality on the atoms it built.
 * Guarantees: all six claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"
#include "lowering.h"
#include "control_forms.h"
#include <inttypes.h>

static mt_atom *sum(void) { return T_ADD(1, 2); }

int main(void)
{
    metta *m = open_engine();
    require("import lib_he", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_he")))));
    static const char *const forms[] = { "quote", "eval" };
    for (size_t i = 0; i < sizeof forms / sizeof *forms; i++)
        check_answers("each form answers as its row says", mt_eval(m, E(forms[i], sum())),
                      answered(control_form_named(forms[i]), sum(), N(C_ADD(1, 2))));
    check_int("unquote undoes a quote", mt_one_int(mt_eval(m, E("unquote", E("quote", sum())))), C_ADD(1, 2));

    const int64_t unquoted = 42;
    char written[32];
    snprintf(written, sizeof written, "(%s %" PRId64 ")", "unquote", unquoted);
    check_answers("repr writes the term", mt_eval(m, E("repr", E("unquote", unquoted))), T(written));

    mt_atom *same = sum(), *three = N(C_ADD(1, 2));
    const mt_atom *compared[] = { same, three };
    for (size_t i = 0; i < 2; i++) {
        mt_atom *left = sum();
        check_answers("noreduce-eq compares the atoms as written",
                      mt_eval(m, E("noreduce-eq", mt_keep(left), mt_keep(compared[i]))), B(mt_eq(left, compared[i])));
        mt_drop(left);
    }
    mt_drop(same), mt_drop(three);
    return done(m);
}
