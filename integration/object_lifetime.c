/* Purpose: Keep native identity and release its payload exactly once.
 * Owns resources: local C handles are released before exit; a failed check
 *   terminates the example process.
 * Guarantees: the assertions below hold [tested: make check; commit=WORKTREE].
 * Open Obligations: None.
 */
#include "common.h"
static unsigned released;
static void release_object(void *p) { ++released; free(p); }
int main(void)
{
    metta *m = open_engine();
    int *value = malloc(sizeof(*value));
    check("allocate payload", value != NULL); *value = 42;
    mt_atom *object = mt_object(value, "Counter", release_object);
    check("construct object", object != NULL);
    check("store borrowed identity", mt_add(m, mt_expr("holds", mt_keep(object))));
    mt_atom *row = mt_one(mt_match(m, mt_expr("holds", mt_var("x"))));
    check("same pointer returned", mt_value(mt_at(row, 1)) == value);
    check_answers("native type reaches engine", mt_eval(m, mt_expr("get-type", mt_keep(object))), "Counter");
    check("remove engine fact", mt_del(m, mt_expr("holds", mt_keep(object))));
    mt_drop(row);
    check("deterministic engine release", mt_object_free(object));
    check("payload released once", released == 1);
    return done(m, "object_lifetime");
}

