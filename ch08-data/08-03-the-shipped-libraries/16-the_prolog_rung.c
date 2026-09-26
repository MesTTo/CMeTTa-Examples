/* Purpose: the underscore spellings the libraries keep beside their MeTTa
 *   names, each held against the same C oracle as its library's own twin:
 *   PCRE2 for regex (pcre_oracle.h), libcrypto for digests
 *   (crypto_oracle.h), strftime for the calendar (time_oracle.h), and the
 *   demo provider's report (conformance_report.h). Where the original asks
 *   whether two spellings agree, C compares the two answers the engine gives
 *   it. Two random draws differing is held against C's own two draws of the
 *   same size from RAND_bytes.
 * Build: cc 16-the_prolog_rung.c $(pkg-config --cflags --libs cmetta
 *   libpcre2-8 libcrypto)
 * Assumes: libpcre2-8 and libcrypto, found through pkg-config; the working
 *   directory is the engine tree.
 * Guarantees: all nineteen claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define _POSIX_C_SOURCE 200809L
#define MT_SHORTHAND
#if __has_include(<pcre2.h>) && __has_include(<openssl/evp.h>)
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_fixtures/pcre_oracle.h"
#include "_fixtures/crypto_oracle.h"
#include "_fixtures/time_oracle.h"
#include "_fixtures/conformance_report.h"
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
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    static const char *const LIBRARIES[] = { "lib_string", "lib_regex", "lib_crypto", "lib_datetime", "lib_conformance" };
    for (size_t i = 0; i < sizeof LIBRARIES / sizeof *LIBRARIES; i++)
        require(LIBRARIES[i], mt_one_truth(mt_eval(m, E("import!", "&self", E("library", mt_sym(LIBRARIES[i]))))));

    /* The regex spellings. */
    pattern needle = compiled(T("(?i)^needle")), starts_x = compiled(T("^x")), digits = compiled(T("\\d+")),
            year = compiled(T("(?<y_I>\\d+)")), colon = compiled(T(":\\s*")), a = compiled(T("a"));
    assert(answers_are(mt_eval(m, E("regex_match", mt_keep(needle.text), T("Needle in a haystack"))), E(B(matches(&needle, "Needle in a haystack"))))
           && "regex_match");
    assert(answers_are(mt_eval(m, E("==", E("regex_match", mt_keep(starts_x.text), T("abc")),
                                      E("re-match", mt_keep(starts_x.text), T("abc")))), E(B(alike(m, E("regex_match", mt_keep(starts_x.text), T("abc")), E("re-match", mt_keep(starts_x.text), T("abc"))))))
           && "is re-match");
    check_all("regex_find", mt_eval(m, E("regex_find", mt_keep(digits.text), T("a1 b22 c333"))),
              found(&digits, "a1 b22 c333", whole));
    assert(answers_are(mt_eval(m, E("regex_captures", mt_keep(year.text), T("n 42"))), E(captures_of(&year, "n 42"))) && "regex_captures");
    assert(answers_are(mt_eval(m, E("regex_split", mt_keep(colon.text), T("Age: 33"))), E(split(&colon, "Age: 33"))) && "regex_split");
    assert(answers_are(mt_eval(m, E("regex_replace", mt_keep(a.text), T("X"), T("banana"))), E(replaced(&a, "banana", "X", 0, false)))
           && "regex_replace");
    assert(answers_are(mt_eval(m, E("regex_replace_all", mt_keep(a.text), T("X"), T("banana"))), E(replaced(&a, "banana", "X", 0, true)))
           && "regex_replace_all");

    /* The crypto spellings. */
    assert(answers_are(mt_eval(m, E("crypto_hash", "sha256", T("text"))), E(digest("SHA256", "text", 4))) && "crypto_hash");
    assert(answers_are(mt_eval(m, E("==", E("crypto_hash", "sha256", T("text")), E("crypto-hash", "sha256", T("text")))), E(B(alike(m, E("crypto_hash", "sha256", T("text")), E("crypto-hash", "sha256", T("text"))))))
           && "is crypto-hash");
    mt_atom *text = digest("SHA256", "text", 4), *other = digest("SHA256", "other", 5);
    assert(answers_are(mt_eval(m, E("==", E("crypto_hash", "sha256", T("text")), E("crypto_hash", "sha256", T("other")))), E(B(mt_eq(text, other))))
           && "different text, a different key");
    mt_drop(text);
    mt_drop(other);
    assert(answers_are(mt_eval(m, E("string-length", E("crypto-random-hex", 8))), E((int64_t)(2 * 8))) && "two hex digits a byte");
    assert(answers_are(mt_eval(m, E("string-length", E("crypto_random_hex", 16))), E((int64_t)(2 * 16))) && "the native spelling too");
    unsigned char first[16], second[16];
    require("RAND_bytes", RAND_bytes(first, sizeof first) == 1 && RAND_bytes(second, sizeof second) == 1);
    assert(answers_are(mt_eval(m, E("==", E("crypto-random-hex", 16), E("crypto-random-hex", 16))), E(B(memcmp(first, second, sizeof first) == 0)))
           && "two draws of sixteen bytes differ");

    /* The calendar spellings, at the epoch. */
    assert(answers_are(mt_eval(m, E("day_of_week", 0)), E(named(0, "%A"))) && "day_of_week");
    assert(answers_are(mt_eval(m, E("format_date", 0, T("%Y-%m-%d"))), E(named(0, "%Y-%m-%d"))) && "format_date");
    assert(answers_are(mt_eval(m, E("==", E("day_of_week", 0), E("day-of-week", 0))), E(B(alike(m, E("day_of_week", 0), E("day-of-week", 0)))))
           && "is day-of-week");
    assert(answers_are(mt_eval(m, E("==", E("format_date", 0, T("%H:%M:%S")), E("format-date", 0, T("%H:%M:%S")))), E(B(alike(m, E("format_date", 0, T("%H:%M:%S")), E("format-date", 0, T("%H:%M:%S"))))))
           && "is format-date");

    /* The conformance kit's Prolog-facing name. */
    mt_list atoms = demo_provider(m);
    assert(answers_are(mt_eval(m, E("metta_check_space_provider", mt_spaceref("&demo_provider"))), E(demo_report(atoms.len)))
           && "metta_check_space_provider");
    assert(answers_are(mt_eval(m, E("==", E("metta_check_space_provider", mt_spaceref("&demo_provider")),
                                    E("check-space-provider", mt_spaceref("&demo_provider")))), E(B(alike(m, E("metta_check_space_provider", mt_spaceref("&demo_provider")),
                                                                                                          E("check-space-provider", mt_spaceref("&demo_provider"))))))
           && "is check-space-provider");
    mt_list_free(atoms);

    pattern *all[] = { &needle, &starts_x, &digits, &year, &colon, &a };
    for (size_t i = 0; i < sizeof all / sizeof *all; i++) release(all[i]);
    mt_close(m);
    return 0;
}
#else
#include <stdio.h>

/* Without PCRE2 and OpenSSL's libcrypto's headers the program only says what it needs. */
int main(void)
{
    fputs("16-the_prolog_rung.c needs PCRE2 and OpenSSL's libcrypto: install its development files, then build with\n"
          "cc 16-the_prolog_rung.c $(pkg-config --cflags --libs cmetta libpcre2-8 libcrypto)\n", stderr);
    return 77;
}
#endif
