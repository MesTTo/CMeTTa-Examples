/* Purpose: reading source without running it. mt_forms() answers every
 *   top-level form of a text as atoms, a `!` form's body included, and writes
 *   nothing; mt_parse() refuses text that is not a whole form.
 * text: the program's subject is MeTTa source text as a reader receives it.
 * Guarantees: two forms read, nothing is stored, a broken form is refused
 *   with a status [tested: make check; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    mt_list forms = mt_forms("(fact 7) !(+ 20 22)");
    check_int("the reader answers two forms", (int64_t)forms.len, 2);
    check("the first is the fact", forms.len == 2 && alpha_equal(forms.items[0], E("fact", 7)));
    check("the directive's body is data, not 42",
          forms.len == 2 && alpha_equal(forms.items[1], E("+", 20, 22)));
    mt_list_free(forms);
    check_int("reading wrote nothing", (int64_t)mt_count(m), 0);

    mt_clear();
    mt_atom *broken = mt_parse("(unfinished");
    check("a broken form is refused with a status", broken == NULL && mt_error() == MT_ERROR);
    mt_clear();
    return done(m);
}
