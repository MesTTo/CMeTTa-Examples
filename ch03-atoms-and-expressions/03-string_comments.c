/* Purpose: inside a string, `;` starts no comment and a lone paren is no
 *   paren. The original is a reader test, so this twin hands the reader each
 *   string's source spelling and checks the text atom it reads, then checks
 *   the value half: each text evaluates to itself. C's own escapes spell the
 *   same bytes MeTTa's do, so each row states both spellings side by side.
 * text: the original tests the MeTTa reader on string literals and comments,
 *   which is source text by definition.
 * Guarantees: all nine strings read and evaluate to themselves, (quote ";")
 *   is ";", a trailing comment is stripped from (= (test-func) result), and
 *   (test-func) is result [tested 2026-09-27T00:35:58+10:00:
 *   make -C extensions/cmetta corpus-check].
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

/* The source spelling the reader sees, and the bytes it must read to. */
static const struct { const char *source, *bytes; } strings[] = {
    { "\")\"",             ")" },
    { "\"(\"",             "(" },
    { "\";\"",             ";" },
    { "\"foo;bar\"",       "foo;bar" },
    { "\";;;\"",           ";;;" },
    { "\";start\"",        ";start" },
    { "\"end;\"",          "end;" },
    { "\"quote: \\\"\"",   "quote: \"" },
    { "\"path\\\\file\"",  "path\\file" },
};

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;

    for (size_t i = 0; i < sizeof strings / sizeof strings[0]; i++) {
        mt_atom *read = mt_parse(strings[i].source);
        assert(atom_is(read, T(strings[i].bytes)) && strings[i].source);
        assert(answers_are(mt_eval(m, T(strings[i].bytes)), E(T(strings[i].bytes)))
               && strings[i].bytes);
    }

    /* quote is an evaluation barrier and no wrapper survives it. */
    assert(answers_are(mt_eval(m, E("quote", T(";"))), E(T(";"))) && "(quote \";\") is \";\"");

    /* A comment after a form is stripped before the form is read. */
    mt_list forms = mt_forms("(= (test-func) result) ; This comment should be stripped");
    assert(forms.len == 1 && "one form survives the comment");
    mt_atom *definition = E("=", E("test-func"), "result");
    assert(forms.len == 1 && mt_alpha_eq(forms.items[0], definition)
           && "and it is the definition");
    mt_list_free(forms);

    require("define test-func", mt_add(m, definition));
    assert(answers_are(mt_eval(m, E("test-func")), E("result")) && "(test-func) is result");
    mt_close(m);
    return 0;
}
