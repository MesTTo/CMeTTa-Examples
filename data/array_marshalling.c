/* Purpose: Transfer an arbitrary-length C array into an expression.
 * Owns resources: local C handles are released before exit; a failed check
 *   terminates the example process.
 * Guarantees: the assertions below hold [tested: make check; commit=WORKTREE].
 * Open Obligations: None.
 */
#include "common.h"
int main(void)
{
    metta *m = open_engine();
    const int64_t values[] = {3, 5, 8, 13};
    size_t count = sizeof(values) / sizeof(values[0]);
    mt_atom **children = mt_calloc(count, sizeof(*children));
    check("allocate child vector", children != NULL);
    for (size_t i = 0; i < count; ++i) children[i] = mt_num(values[i]);
    mt_atom *array = mt_exprv(count, children);
    mt_free(children); /* exprv takes children, but copies the vector. */
    check("array length", mt_len(array) == count);
    for (size_t i = 0; i < count; ++i)
        check("each element round-trips", mt_int(mt_at(array, i)) == values[i]);
    check_answers("array crosses engine", mt_eval(m, mt_expr("superpose", mt_keep(array))), "3 5 8 13");
    mt_drop(array);
    return done(m, "array_marshalling");
}

