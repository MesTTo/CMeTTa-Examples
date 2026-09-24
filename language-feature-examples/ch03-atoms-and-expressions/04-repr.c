/* Purpose: how an atom prints. mt_show() is the engine's own writer, so the
 *   six printed forms of the original are six rows of a C table, each checked
 *   against mt_show() and against the engine's repr, which must agree.
 * text: the original's subject is the text each atom prints as, so the
 *   expected outputs are written as text.
 * Guarantees: each atom prints as the original says, through both doors
 *   [tested: make twins; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    struct { mt_atom *atom; const char *printed; } rows[] = {
        { N(42),                                   "42" },
        /* a string prints WITH its quotes, so the text reads back as text */
        { T("42"),                                 "\"42\"" },
        { E("A", E("B", "C")),                     "(A (B C))" },
        /* five children under no head; "," is only a symbol's name */
        { E("A", E(",", "B", ",", "C", ",")),      "(A (, B , C ,))" },
        /* a symbol that looks like a date is a symbol */
        { S("2025_12_12"),                         "2025_12_12" },
        { mt_unit(),                               "()" },
    };

    for (size_t i = 0; i < sizeof rows / sizeof rows[0]; i++) {
        check_text(rows[i].printed, mt_show(rows[i].atom), rows[i].printed);
        check_answers(rows[i].printed, mt_eval(m, E("repr", rows[i].atom)),
                      T(rows[i].printed));
    }
    return done(m);
}
