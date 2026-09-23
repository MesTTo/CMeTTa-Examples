/* Purpose: Distinguish refusal, empty answers and sticky error state.
 * Owns resources: local C handles are released before exit; a failed check
 *   terminates the example process.
 * Guarantees: the assertions below hold [tested: make check; commit=6022c3f48b6dc64752c6e49cfe9d985c7ac7a4e9].
 * Open Obligations: None.
 */
#include "common.h"
int main(void)
{
    metta *m = open_engine();
    mt_atom *text = mt_text("not an integer");
    check("failed conversion returns zero", mt_int(text) == 0);
    check("conversion records misuse", mt_error() == MT_MISUSE && mt_errmsg() != NULL);
    mt_atom *number = mt_num(42);
    check("success returns value", mt_int(number) == 42);
    check("success preserves prior error", mt_error() == MT_MISUSE);
    mt_clear();
    check_answers("empty result is successful", mt_eval(m, mt_sym("Empty")), "");
    mt_answers *bad = mt_run(m, "!(assertEqual 1 2)");
    check("false claim is an error", bad == NULL && mt_error() == MT_ERROR);
    check("engine provides repair", mt_remedy() != NULL && mt_ground() != NULL);
    mt_clear(); mt_drop(text); mt_drop(number);
    return done(m, "error_handling");
}
