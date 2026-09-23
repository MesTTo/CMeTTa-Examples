/* Purpose: the underscore spellings the libraries keep beside their MeTTa
 *   names, each held against the same C oracle as its library's own twin:
 *   PCRE2 for regex (pcre_oracle.h), libcrypto for digests
 *   (crypto_oracle.h), strftime for the calendar (time_oracle.h), and the
 *   demo provider's report (conformance_report.h). Where the original asks
 *   whether two spellings agree, C compares the two answers the engine gives
 *   it. Two random draws differing is held against C's own two draws of the
 *   same size from RAND_bytes.
 * Assumes: libpcre2-8 and libcrypto, found through pkg-config; the working
 *   directory is the engine tree.
 * Guarantees: all nineteen claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"
#include "pcre_oracle.h"
#include "crypto_oracle.h"
#include "time_oracle.h"
#include "conformance_report.h"
#include <openssl/rand.h>

/* Whether the engine answers two goals alike, C comparing the answers. */
static bool alike(metta *m, mt_atom *left, mt_atom *right)
{
    mt_list a = mt_all(mt_eval(m, left)), b = mt_all(mt_eval(m, right));
    bool equal = mt_ok() && a.len == b.len;
    for (size_t i = 0; equal && i < a.len; i++) equal = mt_eq(a.items[i], b.items[i]);
    mt_list_free(a);
    mt_list_free(b);
    return equal;
}

int main(void)
{
    metta *m = open_engine();
    static const char *const LIBRARIES[] = { "lib_string", "lib_regex", "lib_crypto", "lib_datetime", "lib_conformance" };
    for (size_t i = 0; i < sizeof LIBRARIES / sizeof *LIBRARIES; i++)
        require(LIBRARIES[i], mt_one_truth(mt_eval(m, E("import!", "&self", E("library", mt_sym(LIBRARIES[i]))))));

    /* The regex spellings. */
    pattern needle = compiled(T("(?i)^needle")), starts_x = compiled(T("^x")), digits = compiled(T("\\d+")),
            year = compiled(T("(?<y_I>\\d+)")), colon = compiled(T(":\\s*")), a = compiled(T("a"));
    check_answers("regex_match", mt_eval(m, E("regex_match", mt_keep(needle.text), T("Needle in a haystack"))),
                  B(matches(&needle, "Needle in a haystack")));
    check_answers("is re-match", mt_eval(m, E("==", E("regex_match", mt_keep(starts_x.text), T("abc")),
                                                E("re-match", mt_keep(starts_x.text), T("abc")))),
                  B(alike(m, E("regex_match", mt_keep(starts_x.text), T("abc")), E("re-match", mt_keep(starts_x.text), T("abc")))));
    check_all("regex_find", mt_eval(m, E("regex_find", mt_keep(digits.text), T("a1 b22 c333"))),
              found(&digits, "a1 b22 c333", whole));
    check_answers("regex_captures", mt_eval(m, E("regex_captures", mt_keep(year.text), T("n 42"))), captures_of(&year, "n 42"));
    check_answers("regex_split", mt_eval(m, E("regex_split", mt_keep(colon.text), T("Age: 33"))), split(&colon, "Age: 33"));
    check_answers("regex_replace", mt_eval(m, E("regex_replace", mt_keep(a.text), T("X"), T("banana"))),
                  replaced(&a, "banana", "X", 0, false));
    check_answers("regex_replace_all", mt_eval(m, E("regex_replace_all", mt_keep(a.text), T("X"), T("banana"))),
                  replaced(&a, "banana", "X", 0, true));

    /* The crypto spellings. */
    check_answers("crypto_hash", mt_eval(m, E("crypto_hash", "sha256", T("text"))), digest("SHA256", "text", 4));
    check_answers("is crypto-hash", mt_eval(m, E("==", E("crypto_hash", "sha256", T("text")), E("crypto-hash", "sha256", T("text")))),
                  B(alike(m, E("crypto_hash", "sha256", T("text")), E("crypto-hash", "sha256", T("text")))));
    mt_atom *text = digest("SHA256", "text", 4), *other = digest("SHA256", "other", 5);
    check_answers("different text, a different key",
                  mt_eval(m, E("==", E("crypto_hash", "sha256", T("text")), E("crypto_hash", "sha256", T("other")))),
                  B(mt_eq(text, other)));
    mt_drop(text);
    mt_drop(other);
    check_answers("two hex digits a byte", mt_eval(m, E("string-length", E("crypto-random-hex", 8))), (int64_t)(2 * 8));
    check_answers("the native spelling too", mt_eval(m, E("string-length", E("crypto_random_hex", 16))), (int64_t)(2 * 16));
    unsigned char first[16], second[16];
    require("RAND_bytes", RAND_bytes(first, sizeof first) == 1 && RAND_bytes(second, sizeof second) == 1);
    check_answers("two draws of sixteen bytes differ",
                  mt_eval(m, E("==", E("crypto-random-hex", 16), E("crypto-random-hex", 16))),
                  B(memcmp(first, second, sizeof first) == 0));

    /* The calendar spellings, at the epoch. */
    check_answers("day_of_week", mt_eval(m, E("day_of_week", 0)), named(0, "%A"));
    check_answers("format_date", mt_eval(m, E("format_date", 0, T("%Y-%m-%d"))), named(0, "%Y-%m-%d"));
    check_answers("is day-of-week", mt_eval(m, E("==", E("day_of_week", 0), E("day-of-week", 0))),
                  B(alike(m, E("day_of_week", 0), E("day-of-week", 0))));
    check_answers("is format-date", mt_eval(m, E("==", E("format_date", 0, T("%H:%M:%S")), E("format-date", 0, T("%H:%M:%S")))),
                  B(alike(m, E("format_date", 0, T("%H:%M:%S")), E("format-date", 0, T("%H:%M:%S")))));

    /* The conformance kit's Prolog-facing name. */
    mt_list atoms = demo_provider(m);
    check_answers("metta_check_space_provider", mt_eval(m, E("metta_check_space_provider", mt_spaceref("&demo_provider"))),
                  demo_report(atoms.len));
    check_answers("is check-space-provider",
                  mt_eval(m, E("==", E("metta_check_space_provider", mt_spaceref("&demo_provider")),
                               E("check-space-provider", mt_spaceref("&demo_provider")))),
                  B(alike(m, E("metta_check_space_provider", mt_spaceref("&demo_provider")),
                          E("check-space-provider", mt_spaceref("&demo_provider")))));
    mt_list_free(atoms);

    pattern *all[] = { &needle, &starts_x, &digits, &year, &colon, &a };
    for (size_t i = 0; i < sizeof all / sizeof *all; i++) release(all[i]);
    return done(m);
}
