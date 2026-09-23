/* Purpose: Distinguish symbols, text, numbers and borrowed children.
 * Owns resources: local C handles are released before exit; a failed check
 *   terminates the example process.
 * Guarantees: the assertions below hold [tested: make check; commit=WORKTREE].
 * Open Obligations: None.
 */
#include "common.h"
int main(void)
{
    metta *m = open_engine();
    mt_atom *term = mt_expr("record", mt_text("Ada"), 42, mt_bool(true));
    check("expression kind", mt_kind_of(term) == MT_EXPR);
    check("four children", mt_len(term) == 4);
    check("head is symbol", mt_kind_of(mt_at(term, 0)) == MT_SYMBOL);
    check("name is text", mt_kind_of(mt_at(term, 1)) == MT_TEXT);
    check("integer payload", mt_int(mt_at(term, 2)) == 42);
    check("boolean payload", mt_truth(mt_at(term, 3)));
    mt_atom *held = mt_keep(mt_at(term, 1));
    mt_drop(term);
    check_atom("retained child outlives parent", held, "\"Ada\"");
    mt_drop(held);
    return done(m, "atom_values");
}

