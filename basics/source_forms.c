/* Purpose: Read complete forms without executing directives.
 * Owns resources: local C handles are released before exit; a failed check
 *   terminates the example process.
 * Guarantees: the assertions below hold [tested: make check; commit=WORKTREE].
 * Open Obligations: None.
 */
#include "common.h"
int main(void)
{
    metta *m = open_engine();
    mt_list forms = mt_forms("(fact 7) !(+ 20 22)");
    check("reader retains directive", forms.len == 3);
    check_atom("first form", forms.items[0], "(fact 7)");
    check_atom("directive marker", forms.items[1], "!");
    check("reading has no writes", mt_count(m) == 0);
    mt_list_free(forms);
    mt_atom *invalid = mt_parse("(unfinished");
    check("syntax failure is reported", invalid == NULL && !mt_ok());
    mt_clear();
    return done(m, "source_forms");
}

