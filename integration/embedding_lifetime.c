/* Purpose: Own the runtime inside a pre-existing C application.
 * Owns resources: local C handles are released before exit; a failed check
 *   terminates the example process.
 * Guarantees: the assertions below hold [tested: make check; commit=WORKTREE].
 * Open Obligations: None.
 */
#include "common.h"
int main(void)
{
    int64_t application_total = 10;
    mt_atom *input = mt_num(32); /* C atoms need no running engine. */
    check("pre-boot value", mt_int(input) == 32);
    metta *m = open_engine();
    mt_atom *answer = mt_one(mt_eval(m, mt_expr("+", application_total, input)));
    check("application uses result", mt_int(answer) == 42);
    mt_close(m);
    check("retained atoms outlive runtime", mt_int(answer) == 42 && mt_ok());
    mt_drop(answer);
    return done(NULL, "embedding_lifetime");
}

