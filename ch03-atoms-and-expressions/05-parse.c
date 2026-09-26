/* Purpose: reading text back into an atom. mt_parse() is the engine's reader
 *   and (parse text) is the same reader inside MeTTa; both answer the atom
 *   the text spells, left unevaluated. The last three rows go round the
 *   loop C owns, mt_write_dup() then mt_parsen(), beside the engine's own
 *   (parse (repr x)).
 * text: the original is about parse, whose input is MeTTa source.
 * Guarantees: every reading of the original holds through mt_parse and
 *   through (parse ...), and writing then reading returns each awkward string
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>

/* A door the program needs before it can go on: on refusal, say why and stop. */
#define require(what, ok) \
    ((ok) ? (void)0 : (fprintf(stderr, "%s: %s\n", (what), mt_errmsg()), exit(EXIT_FAILURE)))

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

/* Whether an atom is want up to renaming variables; takes both, and shows
   them when they differ. */
static inline bool atom_is(mt_atom *got, mt_atom *want)
{
    bool holds = got && want && mt_alpha_eq(got, want);
    if (!holds) fprintf(stderr, "  got %s\n  want %s\n", got ? mt_show(got) : "nothing",
                        want ? mt_show(want) : "nothing");
    mt_drop(got);
    mt_drop(want);
    return holds;
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    struct { const char *source; mt_atom *atom; } readings[] = {
        { "A",              S("A") },
        { "(R A B)",        E("R", "A", "B") },
        { "(R A (S B C))",  E("R", "A", E("S", "B", "C")) },
        /* read, not reduced: parse answers the expression, never 42 */
        { "(* 2 21)",       E("*", 2, 21) },
        { "\"42\"",         T("42") },
    };
    for (size_t i = 0; i < sizeof readings / sizeof readings[0]; i++) {
        assert(atom_is(mt_parse(readings[i].source), mt_keep(readings[i].atom))
               && readings[i].source);
        assert(answers_are(mt_eval(m, E("parse", T(readings[i].source))), E(readings[i].atom))
               && readings[i].source);
    }

    /* Backslashes, embedded quotes, and a backslash-n that is two bytes. */
    static const char *const awkward[] = { "C:\\Users\\bob", "say \"hi\"", "a\\nb" };
    for (size_t i = 0; i < sizeof awkward / sizeof awkward[0]; i++) {
        mt_atom *text = T(awkward[i]);
        mt_string written = mt_write_dup(text);
        require("write the text", written.data != NULL);
        assert(atom_is(mt_parsen(written.data, written.len), mt_keep(text)) && awkward[i]);
        mt_free(written.data);
        assert(answers_are(mt_eval(m, E("parse", E("repr", mt_keep(text)))), E(text)) && awkward[i]);
    }
    mt_close(m);
    return 0;
}
