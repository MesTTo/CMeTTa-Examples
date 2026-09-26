/* Purpose: how an atom prints. mt_show() is the engine's own writer, so the
 *   six printed forms of the original are six rows of a C table, each checked
 *   against mt_show() and against the engine's repr, which must agree.
 * text: the original's subject is the text each atom prints as, so the
 *   expected outputs are written as text.
 * Guarantees: each atom prints as the original says, through both doors
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <string.h>

/* Whether a list holds exactly the children of want, in order, each equal
   up to renaming variables; takes both, and shows them when they differ. */
static inline bool list_is(mt_list got, mt_atom *want)
{
    bool holds = mt_ok() && want && got.len == mt_len(want);
    for (size_t i = 0; holds && i < got.len; i++)
        holds = mt_alpha_eq(got.items[i], mt_at(want, i));
    if (!holds) {
        fprintf(stderr, "  got");
        for (size_t i = 0; i < got.len; i++) fprintf(stderr, " %s", mt_show(got.items[i]));
        fprintf(stderr, "\n  want %s\n", want ? mt_show(want) : "nothing");
    }
    mt_list_free(got);
    mt_drop(want);
    return holds;
}

/* Whether a query answers exactly the children of want; takes both. */
static inline bool answers_are(mt_answers *answers, mt_atom *want)
{
    return list_is(mt_all(answers), want);
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
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
        assert(strcmp(mt_show(rows[i].atom), rows[i].printed) == 0 && rows[i].printed);
        assert(answers_are(mt_eval(m, E("repr", rows[i].atom)), E(T(rows[i].printed)))
               && rows[i].printed);
    }
    mt_close(m);
    return 0;
}
