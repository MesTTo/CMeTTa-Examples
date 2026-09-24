/* Purpose: inside a string, `;` starts no comment and a lone paren is no
 *   paren. The original is a reader test, so this twin hands the reader each
 *   string's source spelling and checks the text atom it reads, then checks
 *   the value half: each text evaluates to itself. C's own escapes spell the
 *   same bytes MeTTa's do, so each row states both spellings side by side.
 * text: the original tests the MeTTa reader on string literals and comments,
 *   which is source text by definition.
 * Guarantees: all nine strings read and evaluate to themselves, (quote ";")
 *   is ";", a trailing comment is stripped from (= (test-func) result), and
 *   (test-func) is result [tested: make twins; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

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
    metta *m = open_engine();

    for (size_t i = 0; i < sizeof strings / sizeof strings[0]; i++) {
        mt_atom *read = mt_parse(strings[i].source);
        check_atom(strings[i].source, read, T(strings[i].bytes));
        check_answers(strings[i].bytes, mt_eval(m, T(strings[i].bytes)),
                      T(strings[i].bytes));
    }

    /* quote is an evaluation barrier and no wrapper survives it. */
    check_answers("(quote \";\") is \";\"", mt_eval(m, E("quote", T(";"))), T(";"));

    /* A comment after a form is stripped before the form is read. */
    mt_list forms = mt_forms("(= (test-func) result) ; This comment should be stripped");
    check("one form survives the comment", forms.len == 1);
    mt_atom *definition = E("=", E("test-func"), "result");
    check("and it is the definition",
          forms.len == 1 && mt_alpha_eq(forms.items[0], definition));
    mt_list_free(forms);

    require("define test-func", mt_add(m, definition));
    check_answers("(test-func) is result", mt_eval(m, E("test-func")), "result");
    return done(m);
}
