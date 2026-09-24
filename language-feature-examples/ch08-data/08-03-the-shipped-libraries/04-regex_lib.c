/* Purpose: lib_regex, held against PCRE2 called from C, the library the
 *   engine's binding wraps. Each pattern is compiled once, in UTF mode as the
 *   binding compiles it, from the very text atom the engine is given. Every
 *   scan is pcre2demo.c's global loop, which the binding follows: after an
 *   empty match it retries at the same place anchored and forbidding an
 *   empty match, and only then steps one character, which is where the
 *   original's empty matches and their nonempty alternatives come from. What
 *   the binding adds, C spells too: captures keyed 0, then by group number,
 *   then by name less its type suffix, in standard order; an optional group
 *   that took no part left out; _I read with strtoll, _R as a range in
 *   characters, _T with sscanf; a replacement as a printf format. (?i), \d,
 *   named groups and a lazy *? are PCRE2's own syntax, so the one library
 *   reads both sides. The oracle lives in pcre_oracle.h, which
 *   16-the_prolog_rung shares.
 * Assumes: libpcre2-8, which lib_regex's native object links
 *   [source: lib/lib_regex/vendor/VENDOR.md;
 *   commit=33c2d50c84b24c1a2c906225600e2a4ffdb662f1].
 * Guarantees: all twenty-six claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include "pcre_oracle.h"

int main(void)
{
    metta *m = open_engine();
    require("import lib_regex", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_regex")))));

    pattern needle = compiled(T("(?i)^needle")), starts_x = compiled(T("^x"));
    check_answers("(?i) folds case", mt_eval(m, E("re-match", mt_keep(needle.text), T("Needle in a haystack"))),
                  B(matches(&needle, "Needle in a haystack")));
    check_answers("^ anchors at the start", mt_eval(m, E("re-match", mt_keep(starts_x.text), T("abc"))),
                  B(matches(&starts_x, "abc")));

    pattern digits = compiled(T("\\d+"));
    check_all("re-find answers every match", mt_eval(m, E("re-find", mt_keep(digits.text), T("a1 b22 c333"))),
              found(&digits, "a1 b22 c333", whole));
    pattern date = compiled(T("(?<year_I>\\d\\d\\d\\d)-(?<month_I>\\d\\d)"));
    check_answers("typed named captures", mt_eval(m, E("re-captures", mt_keep(date.text), T("2017-04-20"))),
                  captures_of(&date, "2017-04-20"));
    pattern colon = compiled(T(":\\s*"));
    check_answers("split keeps what it split on", mt_eval(m, E("re-split", mt_keep(colon.text), T("Age: 33"))),
                  split(&colon, "Age: 33"));
    pattern run_of_a = compiled(T("a+"));
    check_answers("replace every run", mt_eval(m, E("re-replace-all", mt_keep(run_of_a.text), T("X"), T("banana"))),
                  replaced(&run_of_a, "banana", "X", 0, true));
    pattern y = compiled(T("(?<y>\\d+)"));
    check_answers("a named group in the template", mt_eval(m, E("re-replace", mt_keep(y.text), T("[$y]"), T("n 42 n"))),
                  replaced(&y, "n 42 n", "[%.*s]", 1, false));

    /* The native spellings, underscores and all. */
    pattern x = compiled(T("x")), comma = compiled(T(",")), group_x = compiled(T("(x)"));
    check_answers("regex_match", mt_eval(m, E("regex_match", mt_keep(starts_x.text), T("xyz"))),
                  B(matches(&starts_x, "xyz")));
    check_all("regex_find", mt_eval(m, E("regex_find", mt_keep(x.text), T("x-x"))), found(&x, "x-x", whole));
    check_answers("regex_captures", mt_eval(m, E("regex_captures", mt_keep(group_x.text), T("x"))),
                  captures_of(&group_x, "x"));
    check_answers("regex_split", mt_eval(m, E("regex_split", mt_keep(comma.text), T("a,b"))), split(&comma, "a,b"));
    check_answers("regex_replace", mt_eval(m, E("regex_replace", mt_keep(x.text), T("y"), T("xx"))),
                  replaced(&x, "xx", "y", 0, false));
    check_answers("regex_replace_all", mt_eval(m, E("regex_replace_all", mt_keep(x.text), T("y"), T("xx"))),
                  replaced(&x, "xx", "y", 0, true));

    /* A compiled pattern is a value: C holds the engine's, as it holds its
       own pcre2_code, and hands it to a matcher where the text went. */
    mt_atom *held = mt_one(mt_eval(m, E("re-compile", mt_keep(digits.text))));
    check("re-compile answers a value C holds by reference", held && mt_kind_of(held) == MT_HANDLE);
    check_all("which re-find takes as it takes text", mt_eval(m, E("re-find", mt_keep(held), T("n7 n8"))),
              found(&digits, "n7 n8", whole));
    mt_drop(held);

    pattern either = compiled(T("a|ab"));
    check_answers("a full match tries every alternative", mt_eval(m, E("re-fullmatch", mt_keep(either.text), T("ab"))),
                  B(whole_match(&either, "ab")));
    check_answers("and covers the whole text", mt_eval(m, E("re-fullmatch", mt_keep(either.text), T("abc"))),
                  B(whole_match(&either, "abc")));
    pattern n_int = compiled(T("(?<n_I>\\d+)"));
    check_all("re-scan answers each match's captures", mt_eval(m, E("re-scan", mt_keep(n_int.text), T("a1 b22"))),
              found(&n_int, "a1 b22", captures));
    pattern any = compiled(T(".")), nothing = compiled(T(""));
    check_all("ranges count characters, not bytes", mt_eval(m, E("re-ranges", mt_keep(any.text), T("é🦊"))),
              found(&any, "é🦊", range));
    check_answers("an empty pattern matches between every character", mt_eval(m, E("re-count", mt_keep(nothing.text), T("é🦊"))),
                  (int64_t)scan(&nothing, "é🦊", NULL, NULL));
    const char *literal = "a.*\\E #";
    pattern ours = compiled(quoted(literal));
    check_answers("an escaped pattern matches its text whole",
                  mt_eval(m, E("re-fullmatch", E("re-escape", T(literal)), T(literal))), B(whole_match(&ours, literal)));
    pattern theirs = compiled(mt_one(mt_eval(m, E("re-escape", T(literal)))));
    check("and the engine's escaping, compiled by C, does too", whole_match(&theirs, literal));

    /* Empty matches keep their nonempty alternatives; unset groups stay out. */
    pattern lazy = compiled(T("a*?"));
    check_all("a lazy star's empty matches and their alternatives", mt_eval(m, E("re-find", mt_keep(lazy.text), T("aa"))),
              found(&lazy, "aa", whole));
    check_answers("an empty split", mt_eval(m, E("re-split", mt_keep(nothing.text), T("a"))), split(&nothing, "a"));
    check_answers("replacing each of them", mt_eval(m, E("re-replace-all", mt_keep(lazy.text), T("X"), T("aa"))),
                  replaced(&lazy, "aa", "X", 0, true));
    pattern optional = compiled(T("((a)?b)"));
    check_answers("an optional group that took no part", mt_eval(m, E("re-captures", mt_keep(optional.text), T("b"))),
                  captures_of(&optional, "b"));
    pattern term = compiled(T("(?<term_T>1-2)"));
    check_answers("a typed capture substitutes its text",
                  mt_eval(m, E("re-replace", mt_keep(term.text), T("$term"), T("before 1-2 after"))),
                  replaced(&term, "before 1-2 after", "%.*s", 1, false));
    pattern span = compiled(T("(?<span_R>é)(?<term_T>1-2)"));
    check_answers("a range and a term", mt_eval(m, E("re-captures", mt_keep(span.text), T("é1-2"))),
                  captures_of(&span, "é1-2"));

    pattern *all[] = { &needle, &starts_x, &digits, &date, &colon, &run_of_a, &y, &x, &comma, &group_x,
                       &either, &n_int, &any, &nothing, &ours, &theirs, &lazy, &optional, &term, &span };
    for (size_t i = 0; i < sizeof all / sizeof *all; i++) release(all[i]);
    return done(m);
}
