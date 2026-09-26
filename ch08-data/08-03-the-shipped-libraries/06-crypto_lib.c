/* Purpose: lib_crypto, held against OpenSSL's libcrypto, the library the
 *   engine's native object calls. Each digest and HMAC is C's own
 *   EVP_Q_digest or EVP_Q_mac over the same bytes, a file's digest over the
 *   bytes C reads back from the file the engine wrote, and a random draw is
 *   checked against the bound C computes for it. A password record is
 *   verified the way C would: split at its dollars, the salt decoded from
 *   base64, PBKDF2-HMAC-SHA512 derived again for the stated iterations and
 *   compared with the digest the record carries.
 * Build: cc 06-crypto_lib.c $(pkg-config --cflags --libs cmetta libcrypto)
 * Assumes: libcrypto 3, found through pkg-config; hex, digests and base64
 *   come from crypto_oracle.h, which 16-the_prolog_rung and 30-encoding_lib
 *   share.
 * Guarantees: all eighteen claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#if __has_include(<openssl/evp.h>)
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_fixtures/crypto_oracle.h"

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

enum { MOST = 64 };

static mt_atom *hmac(const char *algorithm, const void *key, size_t key_n, const void *data, size_t data_n)
{
    unsigned char md[MOST];
    size_t length;
    require("EVP_Q_mac", EVP_Q_mac(NULL, "HMAC", NULL, algorithm, NULL, key, key_n, data, data_n,
                                  md, sizeof md, &length) != NULL);
    return hex(md, length);
}

/* Whether password matches a $pbkdf2-sha512$t=<iterations>$<salt>$<digest>
   record [source: lib/lib_crypto/lib_crypto.pl, 'crypto-password-hash';
   commit=33c2d50c84b24c1a2c906225600e2a4ffdb662f1]. */
static bool verified(const char *password, const char *record)
{
    char copy[4 * MOST];
    snprintf(copy, sizeof copy, "%s", record);
    char *scheme = strtok(copy, "$"), *cost = strtok(NULL, "$"), *salt64 = strtok(NULL, "$"),
         *digest64 = strtok(NULL, "$");
    require("a PBKDF2-SHA512 record", scheme && strcmp(scheme, "pbkdf2-sha512") == 0 && cost &&
                                      strncmp(cost, "t=", 2) == 0 && salt64 && digest64);
    unsigned char salt[MOST], stored[MOST], derived[MOST];
    long salt_n = unbase64(salt64, strlen(salt64), false, salt), stored_n = unbase64(digest64, strlen(digest64), false, stored);
    require("the record's base64 decodes, to a 64-byte digest", salt_n >= 0 && stored_n == 64);
    require("PKCS5_PBKDF2_HMAC",
            PKCS5_PBKDF2_HMAC(password, (int)strlen(password), salt, (int)salt_n, atoi(cost + 2),
                              EVP_sha512(), (int)stored_n, derived));
    return CRYPTO_memcmp(derived, stored, stored_n) == 0;
}

static mt_atom *password_hash(metta *m, mt_atom *goal)
{
    mt_atom *record = mt_one(mt_eval(m, goal));
    require("a record", record && mt_kind_of(record) == MT_TEXT);
    return record;
}

static char *slurp(const char *path, size_t *n)
{
    FILE *f = fopen(path, "rb");
    char *data = f ? calloc(1, 4096) : NULL;
    if (data) *n = fread(data, 1, 4096, f);
    if (f) fclose(f);
    require("C reads what the engine wrote", data != NULL);
    return data;
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_crypto", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_crypto")))));

    const char *hello = "hello";
    assert(answers_are(mt_eval(m, E("crypto-hash", "sha256", T(hello))), E(digest("SHA256", hello, 5))) && "sha256");
    assert(answers_are(mt_eval(m, E("crypto-hash", "sha512", T(hello))), E(digest("SHA512", hello, 5))) && "sha512");
    require("content-key", mt_add(m, E("=", E("content-key", V("text")), E("crypto-hash", "sha256", V("text")))));
    assert(answers_are(mt_eval(m, E("content-key", T(hello))), E(digest("SHA256", hello, 5)))
           && "a content key is its payload's digest");

    /* Text is UTF-8; the byte door keeps every octet. */
    assert(answers_are(mt_eval(m, E("crypto_hash", "sha256", T(hello))), E(digest("SHA256", hello, 5))) && "the native spelling");
    const unsigned char hello_bytes[] = { 'h', 'e', 'l', 'l', 'o' }, key[] = { 0xc3, 0xa9 }, data[] = { 0x00, 0xff };
    assert(answers_are(mt_eval(m, E("crypto-hash-bytes", "sha256", mt_array(5, hello_bytes))), E(digest("SHA256", hello_bytes, 5)))
           && "the octets of hello");
    assert(answers_are(mt_eval(m, E("crypto-hash-bytes", "sha256", mt_unit())), E(digest("SHA256", "", 0))) && "no octets at all");
    const char *jefe = "Jefe", *nothing = "what do ya want for nothing?";
    assert(answers_are(mt_eval(m, E("crypto-hmac", "sha256", T(jefe), T(nothing))), E(hmac("SHA256", jefe, strlen(jefe), nothing, strlen(nothing))))
           && "an HMAC");
    assert(answers_are(mt_eval(m, E("crypto-hmac-bytes", "sha256", mt_array(2, key), mt_array(2, data))), E(hmac("SHA256", key, 2, data, 2)))
           && "an HMAC over bytes, zero among them");

    /* Random draws: C knows each answer's size or bound, not its value. */
    require("import lib_string", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_string")))));
    assert(answers_are(mt_eval(m, E("crypto_random_hex", 0)), E(hex(NULL, 0))) && "zero bytes are no hex");
    assert(answers_are(mt_eval(m, E("string-length", E("crypto-random-hex", 4))), E((int64_t)(2 * 4))) && "two hex digits a byte");
    assert(answers_are(mt_eval(m, E("crypto-random-bytes", 0)), E(mt_unit())) && "zero bytes");
    assert(answers_are(mt_eval(m, E("size-atom", E("crypto-random-bytes", 8))), E((int64_t)8)) && "eight bytes");
    assert(answers_are(mt_eval(m, E("crypto-random-integer", -9, -8)), E((int64_t)-9)) && "a half-open range of one value");
    int64_t low = -20, high = 7, drawn = mt_one_int(mt_eval(m, E("crypto-random-integer", low, high)));
    assert(mt_ok() && low <= drawn && drawn < high && "a draw lies in its half-open range");

    /* Password records, each verified by C from its own fields. */
    mt_atom *record = password_hash(m, E("crypto-password-hash", T("fixture")));
    assert(answers_are(mt_eval(m, E("let", V("record"), E("crypto-password-hash", T("fixture")),
                                    E("crypto-password-verify", T("fixture"), V("record")))), E(B(verified("fixture", mt_name(record)))))
           && "the default cost");
    mt_drop(record);
    record = password_hash(m, E("crypto-password-hash", T("fixture"), 2));
    assert(answers_are(mt_eval(m, E("let", V("record"), E("crypto-password-hash", T("fixture"), 2),
                                    E("crypto-password-verify", T("fixture"), V("record")))), E(B(verified("fixture", mt_name(record)))))
           && "cost two");
    assert(answers_are(mt_eval(m, E("let", V("record"), E("crypto-password-hash", T("fixture"), 2),
                                    E("crypto-password-verify", T("different"), V("record")))), E(B(verified("different", mt_name(record)))))
           && "a wrong password");
    assert(answers_are(mt_eval(m, E("crypto-password-verify", T("fixture"), mt_keep(record))), E(B(true)))
           && "and the record C verified, verified by the engine");
    mt_drop(record);

    /* A file's digest is the digest of the bytes on disk. */
    require("import lib_file", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_file")))));
    mt_atom *path = mt_one(mt_eval(m, E("temp-path!", T("crypto-content"))));
    require("a scratch path", path && mt_kind_of(path) == MT_TEXT);
    require("the engine writes the file", mt_one_truth(mt_eval(m, E("write-file!", mt_keep(path), T(hello)))));
    size_t n = 0;
    char *bytes = slurp(mt_name(path), &n);
    assert(answers_are(mt_eval(m, E("crypto-hash-file!", "sha256", mt_keep(path))), E(digest("SHA256", bytes, n)))
           && "crypto-hash-file!");
    free(bytes);
    require("the file goes", mt_one_truth(mt_eval(m, E("delete-file!", path))));
    mt_close(m);
    return 0;
}
#else
#include <stdio.h>

/* Without OpenSSL's libcrypto's headers the program only says what it needs. */
int main(void)
{
    fputs("06-crypto_lib.c needs OpenSSL's libcrypto: install its development files, then build with\n"
          "cc 06-crypto_lib.c $(pkg-config --cflags --libs cmetta libcrypto)\n", stderr);
    return 77;
}
#endif
