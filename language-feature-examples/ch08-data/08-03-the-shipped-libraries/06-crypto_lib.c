/* Purpose: lib_crypto, held against OpenSSL's libcrypto, the library the
 *   engine's native object calls. Each digest and HMAC is C's own
 *   EVP_Q_digest or EVP_Q_mac over the same bytes, a file's digest over the
 *   bytes C reads back from the file the engine wrote, and a random draw is
 *   checked against the bound C computes for it. A password record is
 *   verified the way C would: split at its dollars, the salt decoded from
 *   base64, PBKDF2-HMAC-SHA512 derived again for the stated iterations and
 *   compared with the digest the record carries.
 * Assumes: libcrypto 3, found through pkg-config; hex, digests and base64
 *   come from crypto_oracle.h, which 16-the_prolog_rung and 30-encoding_lib
 *   share.
 * Guarantees: all eighteen claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"
#include "crypto_oracle.h"

enum { MOST = 64 };

static mt_atom *hmac(const char *algorithm, const void *key, size_t key_n, const void *data, size_t data_n)
{
    unsigned char md[MOST];
    size_t length;
    require("EVP_Q_mac", EVP_Q_mac(NULL, "HMAC", NULL, algorithm, NULL, key, key_n, data, data_n,
                                  md, sizeof md, &length) != NULL);
    return hex(md, length);
}

/* Bytes as the expression of their octets, lib_crypto's byte door. */
static mt_atom *octets(const unsigned char *bytes, size_t n)
{
    mt_atom *kids[MOST];
    for (size_t i = 0; i < n; i++) kids[i] = mt_num(bytes[i]);
    return mt_exprv(n, kids);
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
    metta *m = open_engine();
    require("import lib_crypto", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_crypto")))));

    const char *hello = "hello";
    check_answers("sha256", mt_eval(m, E("crypto-hash", "sha256", T(hello))), digest("SHA256", hello, 5));
    check_answers("sha512", mt_eval(m, E("crypto-hash", "sha512", T(hello))), digest("SHA512", hello, 5));
    require("content-key", mt_lower(m, (content-key $text), (crypto-hash sha256 $text)));
    check_answers("a content key is its payload's digest", mt_eval(m, E("content-key", T(hello))),
                  digest("SHA256", hello, 5));

    /* Text is UTF-8; the byte door keeps every octet. */
    check_answers("the native spelling", mt_eval(m, E("crypto_hash", "sha256", T(hello))), digest("SHA256", hello, 5));
    const unsigned char hello_bytes[] = { 'h', 'e', 'l', 'l', 'o' }, key[] = { 0xc3, 0xa9 }, data[] = { 0x00, 0xff };
    check_answers("the octets of hello", mt_eval(m, E("crypto-hash-bytes", "sha256", octets(hello_bytes, 5))),
                  digest("SHA256", hello_bytes, 5));
    check_answers("no octets at all", mt_eval(m, E("crypto-hash-bytes", "sha256", mt_unit())), digest("SHA256", "", 0));
    const char *jefe = "Jefe", *nothing = "what do ya want for nothing?";
    check_answers("an HMAC", mt_eval(m, E("crypto-hmac", "sha256", T(jefe), T(nothing))),
                  hmac("SHA256", jefe, strlen(jefe), nothing, strlen(nothing)));
    check_answers("an HMAC over bytes, zero among them",
                  mt_eval(m, E("crypto-hmac-bytes", "sha256", octets(key, 2), octets(data, 2))),
                  hmac("SHA256", key, 2, data, 2));

    /* Random draws: C knows each answer's size or bound, not its value. */
    require("import lib_string", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_string")))));
    check_answers("zero bytes are no hex", mt_eval(m, E("crypto_random_hex", 0)), hex(NULL, 0));
    check_answers("two hex digits a byte", mt_eval(m, E("string-length", E("crypto-random-hex", 4))), (int64_t)(2 * 4));
    check_answers("zero bytes", mt_eval(m, E("crypto-random-bytes", 0)), octets(NULL, 0));
    check_answers("eight bytes", mt_eval(m, E("size-atom", E("crypto-random-bytes", 8))), (int64_t)8);
    check_answers("a half-open range of one value", mt_eval(m, E("crypto-random-integer", -9, -8)), (int64_t)-9);
    int64_t low = -20, high = 7, drawn = mt_one_int(mt_eval(m, E("crypto-random-integer", low, high)));
    check("a draw lies in its half-open range", mt_ok() && low <= drawn && drawn < high);

    /* Password records, each verified by C from its own fields. */
    mt_atom *record = password_hash(m, E("crypto-password-hash", T("fixture")));
    check_answers("the default cost",
                  mt_eval(m, E("let", V("record"), E("crypto-password-hash", T("fixture")),
                               E("crypto-password-verify", T("fixture"), V("record")))),
                  B(verified("fixture", mt_name(record))));
    mt_drop(record);
    record = password_hash(m, E("crypto-password-hash", T("fixture"), 2));
    check_answers("cost two",
                  mt_eval(m, E("let", V("record"), E("crypto-password-hash", T("fixture"), 2),
                               E("crypto-password-verify", T("fixture"), V("record")))),
                  B(verified("fixture", mt_name(record))));
    check_answers("a wrong password",
                  mt_eval(m, E("let", V("record"), E("crypto-password-hash", T("fixture"), 2),
                               E("crypto-password-verify", T("different"), V("record")))),
                  B(verified("different", mt_name(record))));
    check_answers("and the record C verified, verified by the engine",
                  mt_eval(m, E("crypto-password-verify", T("fixture"), mt_keep(record))), B(true));
    mt_drop(record);

    /* A file's digest is the digest of the bytes on disk. */
    require("import lib_file", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_file")))));
    mt_atom *path = mt_one(mt_eval(m, E("temp-path!", T("crypto-content"))));
    require("a scratch path", path && mt_kind_of(path) == MT_TEXT);
    require("the engine writes the file", mt_one_truth(mt_eval(m, E("write-file!", mt_keep(path), T(hello)))));
    size_t n = 0;
    char *bytes = slurp(mt_name(path), &n);
    check_answers("crypto-hash-file!", mt_eval(m, E("crypto-hash-file!", "sha256", mt_keep(path))),
                  digest("SHA256", bytes, n));
    free(bytes);
    require("the file goes", mt_one_truth(mt_eval(m, E("delete-file!", path))));
    return done(m);
}
