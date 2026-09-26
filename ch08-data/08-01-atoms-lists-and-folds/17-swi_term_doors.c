/* Purpose: five doors onto SWI's own term operations, with C doing the
 *   string work beside them. atom_chars takes a written form apart into
 *   one-character symbols, which C does by walking the string; atom_concat
 *   joins two written forms into a symbol, which C does with snprintf.
 *   copy_term renames, term_hash is defined for ground terms only, and
 *   pretty-atom is the writer, all checked as the original checks them.
 * Guarantees: all twenty-one claims of the original hold
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

/* The one-character symbols of `text`. */
static mt_atom *chars(const char *text)
{
    mt_atom *kids[16];
    size_t n = strlen(text);
    for (size_t i = 0; i < n; i++) kids[i] = mt_sym((char[]){ text[i], '\0' });
    return mt_exprv(n, kids);
}

static mt_atom *joined(const char *a, const char *b)
{
    char both[32];
    snprintf(both, sizeof both, "%s%s", a, b);
    return S(both);
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    assert(answers_are(mt_eval(m, E("atom_chars", "abc")), E(chars("abc"))) && "a name comes apart");
    assert(answers_are(mt_eval(m, E("atom_chars", T("abc"))), E(chars("abc"))) && "so does a string");
    char digits[8];
    assert(answers_are(mt_eval(m, E("size-atom", E("atom_chars", 123))), E((int64_t)snprintf(digits, sizeof digits, "%d", 123)))
           && "a number by its written form");
    assert(answers_are(mt_eval(m, E("==", E("atom_chars", 123), E(1, 2, 3))), E(B(false))) && "into symbols, not numbers");
    assert(answers_are(mt_eval(m, E("atom_concat", E("car-atom", E("atom_chars", "abc")), "bc")), E(joined("a", "bc")))
           && "the first character joins back");

    assert(answers_are(mt_eval(m, E("atom_concat", "ab", "cd")), E(joined("ab", "cd"))) && "two names join");
    assert(answers_are(mt_eval(m, E("atom_concat", T("ab"), "cd")), E(joined("ab", "cd"))) && "a string joins into a name");
    assert(answers_are(mt_eval(m, E("atom_concat", "ab", 1)), E(joined("ab", "1"))) && "so does a number");
    assert(answers_are(mt_eval(m, E("==", E("atom_concat", T("ab"), T("cd")), T("abcd"))), E(B(false))) && "the join is a name, not a string");

    assert(answers_are(mt_eval(m, E("=alpha", E("copy_term", E("foo", V("x"), V("y"))), E("foo", V("a"), V("b")))), E(B(true))) && "copy_term renames");
    assert(answers_are(mt_eval(m, E("let", E(V("p"), V("q")), E("copy_term", E(V("x"), V("x"))), E("=?", V("p"), V("q")))), E(B(true))) && "keeping sharing");
    assert(answers_are(mt_eval(m, E("let", E(V("p"), V("q")), E("copy_term", E(V("x"), V("y"))), E("==", V("p"), V("q")))), E(B(false))) && "and distinctness");

    assert(answers_are(mt_eval(m, E("==", E("term_hash", E("foo", 1)), E("term_hash", E("foo", 1)))), E(B(true))) && "equal terms hash alike");
    assert(answers_are(mt_eval(m, E("==", E("term_hash", E("foo", 1)), E("term_hash", E("foo", 2)))), E(B(false))) && "different terms do not");
    assert(answers_are(mt_eval(m, E("==", E("+", 0, E("term_hash", E("foo", 1))), E("term_hash", E("foo", 1)))), E(B(true))) && "a hash is a number");
    assert(answers_are(mt_eval(m, E("is-ground", E("foo", 1))), E(B(true))) && "(foo 1) is ground");
    assert(answers_are(mt_eval(m, E("is-ground", E("foo", V("x")))), E(B(false))) && "(foo $x) is not");
    assert(answers_are(mt_eval(m, E("let", V("h"), E("term_hash", E("foo", V("x"))), E("is-var", V("h")))), E(B(true))) && "and hashes to nothing bound");

    assert(answers_are(mt_eval(m, E("pretty-atom", E("noeval", E("=", E("f", 1), E("+", 1, 1))))), E(T("(= (f 1) (+ 1 1))")))
           && "pretty-atom writes source");
    assert(answers_are(mt_eval(m, E("==", E("pretty-atom", E("noeval", E("f", V("x")))),
                                     E("pretty-atom", E("noeval", E("f", V("y")))))), E(B(true)))
           && "renaming variables alike");
    assert(answers_are(mt_eval(m, E("pretty-atom", 1)), E(T("1"))) && "(pretty-atom 1)");
    mt_close(m);
    return 0;
}
