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
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* What a console sees, and what it should do with it. */
static void verdict(metta *m, const char *typed, mt_atom *want)
{
    assert(answers_are(mt_eval(m, E("parse-command", T(typed))), E(want)) && typed);
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
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
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
    assert(refused(mt_eval(m, E("parse-command", T("(f a))"))))
           && "a surplus bracket is refused");

    /* sread, which parse-command is written over, and C's mt_parse are the
       same reader: they agree wherever the text parses. */
    static const char *const parses[] = { "(f a)", "hello", "42", "(f \"a)b\")" };
    for (size_t i = 0; i < sizeof parses / sizeof parses[0]; i++) {
        assert(answers_are(mt_eval(m, E("repr", E("sread", T(parses[i])))), E(T(parses[i])))
               && parses[i]);
        mt_atom *read = mt_parse(parses[i]);
        assert(strcmp(mt_show(read), parses[i]) == 0 && parses[i]);
        mt_drop(read);
    }

    /* Which is why parse-command exists: sread refuses a half-typed form and a
       wrong one alike. */
    assert(refused(mt_eval(m, E("sread", T("(f a")))) && "sread refuses a half-typed form");
    assert(refused(mt_eval(m, E("sread", T("(f a))")))) && "sread refuses a surplus bracket");
    mt_clear();
    assert(mt_parse("(f a") == NULL && !mt_ok() && "mt_parse refuses a half-typed form too");
    mt_clear();
    verdict(m, "(f a", S("incomplete"));
    mt_close(m);
    return 0;
}
