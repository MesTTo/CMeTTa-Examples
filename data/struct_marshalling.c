/* Purpose: Map explicit C struct fields to a typed record term.
 * Owns resources: local C handles are released before exit; a failed check
 *   terminates the example process.
 * Guarantees: the assertions below hold [tested: make check; commit=WORKTREE].
 * Open Obligations: None.
 */
#include "common.h"
typedef struct { const char *name; int64_t age; bool active; } person;
int main(void)
{
    metta *m = open_engine();
    person ada = {"Ada", 36, true};
    check("store fields", mt_add(m, mt_expr("Person", mt_text(ada.name),
                                           ada.age, mt_bool(ada.active))));
    mt_atom *row = mt_one(mt_match(m, mt_expr("Person", mt_var("n"), mt_var("a"), mt_var("live"))));
    check("field count", mt_len(row) == 4);
    person read = {mt_name(mt_at(row, 1)), mt_int(mt_at(row, 2)), mt_truth(mt_at(row, 3))};
    check("struct round-trip", strcmp(read.name, ada.name) == 0 && read.age == 36 && read.active);
    mt_drop(row); /* read.name is borrowed and no longer usable. */
    return done(m, "struct_marshalling");
}

