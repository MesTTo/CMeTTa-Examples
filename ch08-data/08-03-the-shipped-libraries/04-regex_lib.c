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
 * Build: cc 04-regex_lib.c $(pkg-config --cflags --libs cmetta libpcre2-8)
 * Assumes: libpcre2-8, which lib_regex's native object links
 *   [source: lib/lib_regex/vendor/VENDOR.md;
 *   commit=33c2d50c84b24c1a2c906225600e2a4ffdb662f1].
 * Guarantees: all twenty-six claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#if __has_include(<pcre2.h>)
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include "_fixtures/pcre_oracle.h"

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_regex", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_regex")))));

    pattern needle = compiled(T("(?i)^needle")), starts_x = compiled(T("^x"));
    assert(answers_are(mt_eval(m, E("re-match", mt_keep(needle.text), T("Needle in a haystack"))), E(B(matches(&needle, "Needle in a haystack"))))
           && "(?i) folds case");
    assert(answers_are(mt_eval(m, E("re-match", mt_keep(starts_x.text), T("abc"))), E(B(matches(&starts_x, "abc"))))
           && "^ anchors at the start");

    pattern digits = compiled(T("\\d+"));
    check_all("re-find answers every match", mt_eval(m, E("re-find", mt_keep(digits.text), T("a1 b22 c333"))),
              found(&digits, "a1 b22 c333", whole));
    pattern date = compiled(T("(?<year_I>\\d\\d\\d\\d)-(?<month_I>\\d\\d)"));
    assert(answers_are(mt_eval(m, E("re-captures", mt_keep(date.text), T("2017-04-20"))), E(captures_of(&date, "2017-04-20")))
           && "typed named captures");
    pattern colon = compiled(T(":\\s*"));
    assert(answers_are(mt_eval(m, E("re-split", mt_keep(colon.text), T("Age: 33"))), E(split(&colon, "Age: 33")))
           && "split keeps what it split on");
    pattern run_of_a = compiled(T("a+"));
    assert(answers_are(mt_eval(m, E("re-replace-all", mt_keep(run_of_a.text), T("X"), T("banana"))), E(replaced(&run_of_a, "banana", "X", 0, true)))
           && "replace every run");
    pattern y = compiled(T("(?<y>\\d+)"));
    assert(answers_are(mt_eval(m, E("re-replace", mt_keep(y.text), T("[$y]"), T("n 42 n"))), E(replaced(&y, "n 42 n", "[%.*s]", 1, false)))
           && "a named group in the template");

    /* The native spellings, underscores and all. */
    pattern x = compiled(T("x")), comma = compiled(T(",")), group_x = compiled(T("(x)"));
    assert(answers_are(mt_eval(m, E("regex_match", mt_keep(starts_x.text), T("xyz"))), E(B(matches(&starts_x, "xyz"))))
           && "regex_match");
    check_all("regex_find", mt_eval(m, E("regex_find", mt_keep(x.text), T("x-x"))), found(&x, "x-x", whole));
    assert(answers_are(mt_eval(m, E("regex_captures", mt_keep(group_x.text), T("x"))), E(captures_of(&group_x, "x")))
           && "regex_captures");
    assert(answers_are(mt_eval(m, E("regex_split", mt_keep(comma.text), T("a,b"))), E(split(&comma, "a,b"))) && "regex_split");
    assert(answers_are(mt_eval(m, E("regex_replace", mt_keep(x.text), T("y"), T("xx"))), E(replaced(&x, "xx", "y", 0, false)))
           && "regex_replace");
    assert(answers_are(mt_eval(m, E("regex_replace_all", mt_keep(x.text), T("y"), T("xx"))), E(replaced(&x, "xx", "y", 0, true)))
           && "regex_replace_all");

    /* A compiled pattern is a value: C holds the engine's, as it holds its
       own pcre2_code, and hands it to a matcher where the text went. */
    mt_atom *held = mt_one(mt_eval(m, E("re-compile", mt_keep(digits.text))));
    assert(held && mt_kind_of(held) == MT_HANDLE && "re-compile answers a value C holds by reference");
    check_all("which re-find takes as it takes text", mt_eval(m, E("re-find", mt_keep(held), T("n7 n8"))),
              found(&digits, "n7 n8", whole));
    mt_drop(held);

    pattern either = compiled(T("a|ab"));
    assert(answers_are(mt_eval(m, E("re-fullmatch", mt_keep(either.text), T("ab"))), E(B(whole_match(&either, "ab"))))
           && "a full match tries every alternative");
    assert(answers_are(mt_eval(m, E("re-fullmatch", mt_keep(either.text), T("abc"))), E(B(whole_match(&either, "abc"))))
           && "and covers the whole text");
    pattern n_int = compiled(T("(?<n_I>\\d+)"));
    check_all("re-scan answers each match's captures", mt_eval(m, E("re-scan", mt_keep(n_int.text), T("a1 b22"))),
              found(&n_int, "a1 b22", captures));
    pattern any = compiled(T(".")), nothing = compiled(T(""));
    check_all("ranges count characters, not bytes", mt_eval(m, E("re-ranges", mt_keep(any.text), T("é🦊"))),
              found(&any, "é🦊", range));
    assert(answers_are(mt_eval(m, E("re-count", mt_keep(nothing.text), T("é🦊"))), E((int64_t)scan(&nothing, "é🦊", NULL, NULL)))
           && "an empty pattern matches between every character");
    const char *literal = "a.*\\E #";
    pattern ours = compiled(quoted(literal));
    assert(answers_are(mt_eval(m, E("re-fullmatch", E("re-escape", T(literal)), T(literal))), E(B(whole_match(&ours, literal))))
           && "an escaped pattern matches its text whole");
    pattern theirs = compiled(mt_one(mt_eval(m, E("re-escape", T(literal)))));
    assert(whole_match(&theirs, literal) && "and the engine's escaping, compiled by C, does too");

    /* Empty matches keep their nonempty alternatives; unset groups stay out. */
    pattern lazy = compiled(T("a*?"));
    check_all("a lazy star's empty matches and their alternatives", mt_eval(m, E("re-find", mt_keep(lazy.text), T("aa"))),
              found(&lazy, "aa", whole));
    assert(answers_are(mt_eval(m, E("re-split", mt_keep(nothing.text), T("a"))), E(split(&nothing, "a"))) && "an empty split");
    assert(answers_are(mt_eval(m, E("re-replace-all", mt_keep(lazy.text), T("X"), T("aa"))), E(replaced(&lazy, "aa", "X", 0, true)))
           && "replacing each of them");
    pattern optional = compiled(T("((a)?b)"));
    assert(answers_are(mt_eval(m, E("re-captures", mt_keep(optional.text), T("b"))), E(captures_of(&optional, "b")))
           && "an optional group that took no part");
    pattern term = compiled(T("(?<term_T>1-2)"));
    assert(answers_are(mt_eval(m, E("re-replace", mt_keep(term.text), T("$term"), T("before 1-2 after"))), E(replaced(&term, "before 1-2 after", "%.*s", 1, false)))
           && "a typed capture substitutes its text");
    pattern span = compiled(T("(?<span_R>é)(?<term_T>1-2)"));
    assert(answers_are(mt_eval(m, E("re-captures", mt_keep(span.text), T("é1-2"))), E(captures_of(&span, "é1-2")))
           && "a range and a term");

    pattern *all[] = { &needle, &starts_x, &digits, &date, &colon, &run_of_a, &y, &x, &comma, &group_x,
                       &either, &n_int, &any, &nothing, &ours, &theirs, &lazy, &optional, &term, &span };
    for (size_t i = 0; i < sizeof all / sizeof *all; i++) release(all[i]);
    mt_close(m);
    return 0;
}
#else
#include <stdio.h>

/* Without PCRE2's headers the program only says what it needs. */
int main(void)
{
    fputs("04-regex_lib.c needs PCRE2: install its development files, then build with\n"
          "cc 04-regex_lib.c $(pkg-config --cflags --libs cmetta libpcre2-8)\n", stderr);
    return 77;
}
#endif
