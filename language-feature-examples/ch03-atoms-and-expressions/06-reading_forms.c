/* Purpose: still typing, or wrong? (parse-command text) answers
 *   (complete term), incomplete, or refuses, so a console can tell a
 *   half-typed form from a broken one. In C the refusal is a status: the
 *   cursor answers nothing and mt_error() says MT_ERROR with the reader's
 *   words, which is how every door here reports a failure.
 * text: the original's subject is source text a console receives, handed to
 *   parse-command and sread as data.
 * Guarantees: every verdict of the original holds, a surplus bracket is a
 *   refusal rather than incomplete, and mt_parse(), the reader sread is,
 *   agrees on every text that parses and refuses the other two
 *   [tested: make twins; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

/* What a console sees, and what it should do with it. */
static void verdict(metta *m, const char *typed, mt_atom *want)
{
    check_answers(typed, mt_eval(m, E("parse-command", T(typed))), want);
}

/* A refusal, told apart from an empty answer by the error status. */
static bool refused(mt_answers *answers)
{
    mt_clear();
    mt_atom *any = mt_first(answers);
    bool refusal = any == NULL && mt_error() == MT_ERROR;
    mt_drop(any);
    mt_clear();
    return refusal;
}

int main(void)
{
    metta *m = open_engine();
    require("import lib_he",
            mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_he")))));

    verdict(m, "(f a)", E("complete", E("f", "a")));

    /* Still typing: more text could finish any of these. */
    verdict(m, "(f a", S("incomplete"));
    verdict(m, "(a (b (c", S("incomplete"));
    verdict(m, "(= (f $x)", S("incomplete"));

    /* An empty line re-prompts rather than erroring. */
    verdict(m, "", S("incomplete"));
    verdict(m, "   ", S("incomplete"));
    verdict(m, "; only a comment", S("incomplete"));

    /* A bare atom is a whole form. */
    verdict(m, "hello", E("complete", "hello"));

    /* A bracket inside a string or a comment does not count. */
    verdict(m, "(f \"a)b\")", E("complete", E("f", T("a)b"))));
    verdict(m, "(f a) ; )))", E("complete", E("f", "a")));

    /* An unterminated string is incomplete: a string may span lines. */
    verdict(m, "(f \"a", S("incomplete"));

    /* One bracket too many is wrong, not unfinished: no typing repairs it. */
    check("a surplus bracket is refused",
          refused(mt_eval(m, E("parse-command", T("(f a))")))));

    /* sread, which parse-command is written over, and C's mt_parse are the
       same reader: they agree wherever the text parses. */
    static const char *const parses[] = { "(f a)", "hello", "42", "(f \"a)b\")" };
    for (size_t i = 0; i < sizeof parses / sizeof parses[0]; i++) {
        check_answers(parses[i], mt_eval(m, E("repr", E("sread", T(parses[i])))),
                      T(parses[i]));
        mt_atom *read = mt_parse(parses[i]);
        check_text(parses[i], mt_show(read), parses[i]);
        mt_drop(read);
    }

    /* Which is why parse-command exists: sread refuses a half-typed form and a
       wrong one alike. */
    check("sread refuses a half-typed form", refused(mt_eval(m, E("sread", T("(f a")))));
    check("sread refuses a surplus bracket", refused(mt_eval(m, E("sread", T("(f a))")))));
    mt_clear();
    check("mt_parse refuses a half-typed form too", mt_parse("(f a") == NULL && !mt_ok());
    mt_clear();
    verdict(m, "(f a", S("incomplete"));
    return done(m);
}
