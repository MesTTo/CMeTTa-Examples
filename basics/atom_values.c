/* Purpose: an atom is a C value. Symbols, text, numbers and booleans are
 *   distinct kinds, children are borrowed from their parent, and a child kept
 *   with mt_keep() outlives the parent it came from.
 * Guarantees: each kind and payload reads back as built [tested: make check;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    mt_atom *record = E("record", T("Ada"), 42, B(true));
    check("an expression", mt_kind_of(record) == MT_EXPR);
    check_int("of four children", (int64_t)mt_len(record), 4);
    check("the head is a symbol", mt_kind_of(mt_at(record, 0)) == MT_SYMBOL);
    check("the name is text, not a symbol", mt_kind_of(mt_at(record, 1)) == MT_TEXT);
    check_int("the number is an integer", mt_int(mt_at(record, 2)), 42);
    check("the flag is a boolean", mt_kind_of(mt_at(record, 3)) == MT_BOOL &&
                                   mt_truth(mt_at(record, 3)));

    mt_atom *name = mt_keep(mt_at(record, 1));
    mt_drop(record);
    check_atom("a kept child outlives its parent", name, T("Ada"));

    /* The engine hands the same kinds back. */
    check_answers("a record evaluates to itself",
                  mt_eval(m, E("record", T("Ada"), 42, B(true))),
                  E("record", T("Ada"), 42, B(true)));
    return done(m);
}
