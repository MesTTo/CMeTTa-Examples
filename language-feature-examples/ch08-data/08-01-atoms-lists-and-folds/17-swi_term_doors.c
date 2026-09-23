/* Purpose: five doors onto SWI's own term operations, with C doing the
 *   string work beside them. atom_chars takes a written form apart into
 *   one-character symbols, which C does by walking the string; atom_concat
 *   joins two written forms into a symbol, which C does with snprintf.
 *   copy_term renames, term_hash is defined for ground terms only, and
 *   pretty-atom is the writer, all checked as the original checks them.
 * Guarantees: all twenty-one claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

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
    metta *m = open_engine();
    check_answers("a name comes apart", mt_eval(m, E("atom_chars", "abc")), chars("abc"));
    check_answers("so does a string", mt_eval(m, E("atom_chars", T("abc"))), chars("abc"));
    char digits[8];
    check_answers("a number by its written form", mt_eval(m, E("size-atom", E("atom_chars", 123))),
                  (int64_t)snprintf(digits, sizeof digits, "%d", 123));
    check_answers("into symbols, not numbers", mt_eval(m, E("==", E("atom_chars", 123), E(1, 2, 3))), B(false));
    check_answers("the first character joins back", mt_eval(m, E("atom_concat", E("car-atom", E("atom_chars", "abc")), "bc")),
                  joined("a", "bc"));

    check_answers("two names join", mt_eval(m, E("atom_concat", "ab", "cd")), joined("ab", "cd"));
    check_answers("a string joins into a name", mt_eval(m, E("atom_concat", T("ab"), "cd")), joined("ab", "cd"));
    check_answers("so does a number", mt_eval(m, E("atom_concat", "ab", 1)), joined("ab", "1"));
    check_answers("the join is a name, not a string", mt_eval(m, E("==", E("atom_concat", T("ab"), T("cd")), T("abcd"))), B(false));

    check_answers("copy_term renames", mt_eval(m, E("=alpha", E("copy_term", E("foo", V("x"), V("y"))), E("foo", V("a"), V("b")))), B(true));
    check_answers("keeping sharing", mt_eval(m, E("let", E(V("p"), V("q")), E("copy_term", E(V("x"), V("x"))), E("=?", V("p"), V("q")))), B(true));
    check_answers("and distinctness", mt_eval(m, E("let", E(V("p"), V("q")), E("copy_term", E(V("x"), V("y"))), E("==", V("p"), V("q")))), B(false));

    check_answers("equal terms hash alike", mt_eval(m, E("==", E("term_hash", E("foo", 1)), E("term_hash", E("foo", 1)))), B(true));
    check_answers("different terms do not", mt_eval(m, E("==", E("term_hash", E("foo", 1)), E("term_hash", E("foo", 2)))), B(false));
    check_answers("a hash is a number", mt_eval(m, E("==", E("+", 0, E("term_hash", E("foo", 1))), E("term_hash", E("foo", 1)))), B(true));
    check_answers("(foo 1) is ground", mt_eval(m, E("is-ground", E("foo", 1))), B(true));
    check_answers("(foo $x) is not", mt_eval(m, E("is-ground", E("foo", V("x")))), B(false));
    check_answers("and hashes to nothing bound", mt_eval(m, E("let", V("h"), E("term_hash", E("foo", V("x"))), E("is-var", V("h")))), B(true));

    check_answers("pretty-atom writes source", mt_eval(m, E("pretty-atom", E("noeval", E("=", E("f", 1), E("+", 1, 1))))),
                  T("(= (f 1) (+ 1 1))"));
    check_answers("renaming variables alike", mt_eval(m, E("==", E("pretty-atom", E("noeval", E("f", V("x")))),
                                                            E("pretty-atom", E("noeval", E("f", V("y")))))), B(true));
    check_answers("(pretty-atom 1)", mt_eval(m, E("pretty-atom", 1)), T("1"));
    return done(m);
}
