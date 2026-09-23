/* Purpose: Read complete forms without executing directives.
 * Owns resources: local C handles are released before exit; a failed check
 *   terminates the example process.
 * Guarantees: the assertions below hold [tested: make check; commit=6022c3f48b6dc64752c6e49cfe9d985c7ac7a4e9].
 * Open Obligations: None.
 */
#include "common.h"
int main(void)
{
    metta *m = open_engine();
    mt_list forms = mt_forms("(fact 7) !(+ 20 22)");
    check("reader returns two forms", forms.len == 2);
    check_atom("first form", forms.items[0], "(fact 7)");
    check_atom("directive body is data", forms.items[1], "(+ 20 22)");
    check("reading has no writes", mt_count(m) == 0);
    mt_list_free(forms);
    mt_atom *invalid = mt_parse("(unfinished");
    check("syntax failure is reported", invalid == NULL && !mt_ok());
    mt_clear();
    return done(m, "source_forms");
}
