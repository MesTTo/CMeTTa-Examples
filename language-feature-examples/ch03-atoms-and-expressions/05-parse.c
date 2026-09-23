/* Purpose: reading text back into an atom. mt_parse() is the engine's reader
 *   and (parse text) is the same reader inside MeTTa; both answer the atom
 *   the text spells, left unevaluated. The last three rows go round the
 *   loop C owns, mt_write_dup() then mt_parsen(), beside the engine's own
 *   (parse (repr x)).
 * text: the original is about parse, whose input is MeTTa source.
 * Guarantees: every reading of the original holds through mt_parse and
 *   through (parse ...), and writing then reading returns each awkward string
 *   [tested: make twins; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    struct { const char *source; mt_atom *atom; } readings[] = {
        { "A",              S("A") },
        { "(R A B)",        E("R", "A", "B") },
        { "(R A (S B C))",  E("R", "A", E("S", "B", "C")) },
        /* read, not reduced: parse answers the expression, never 42 */
        { "(* 2 21)",       E("*", 2, 21) },
        { "\"42\"",         T("42") },
    };
    for (size_t i = 0; i < sizeof readings / sizeof readings[0]; i++) {
        check_atom(readings[i].source, mt_parse(readings[i].source),
                   mt_keep(readings[i].atom));
        check_answers(readings[i].source,
                      mt_eval(m, E("parse", T(readings[i].source))),
                      readings[i].atom);
    }

    /* Backslashes, embedded quotes, and a backslash-n that is two bytes. */
    static const char *const awkward[] = { "C:\\Users\\bob", "say \"hi\"", "a\\nb" };
    for (size_t i = 0; i < sizeof awkward / sizeof awkward[0]; i++) {
        mt_atom *text = T(awkward[i]);
        mt_string written = mt_write_dup(text);
        require("write the text", written.data != NULL);
        check_atom(awkward[i], mt_parsen(written.data, written.len), mt_keep(text));
        mt_free(written.data);
        check_answers(awkward[i], mt_eval(m, E("parse", E("repr", mt_keep(text)))), text);
    }
    return done(m);
}
