/* Purpose: libcrypto as the oracle for the libraries that encode and digest
 *   bytes, shared by 06-crypto_lib, 16-the_prolog_rung and 30-encoding_lib:
 *   EVP_Q_digest over the same bytes, the lowercase hex lib_crypto answers
 *   in, and base64 by EVP_EncodeBlock and EVP_DecodeBlock in the standard
 *   alphabet or url's. Static inline, so a twin that uses part of it
 *   compiles clean.
 * Assumes: libcrypto 3, and common.h included first with MT_SHORTHAND.
 */
#ifndef CRYPTO_ORACLE_H
#define CRYPTO_ORACLE_H
#include <openssl/crypto.h>
#include <openssl/evp.h>

enum { DIGEST_MOST = 64, BASE64_MOST = 128 };

/* Bytes as lowercase hex, the spelling lib_crypto answers in. */
static inline mt_atom *hex(const unsigned char *bytes, size_t n)
{
    char out[2 * DIGEST_MOST + 1];
    for (size_t i = 0; i < n; i++) snprintf(out + 2 * i, 3, "%02x", bytes[i]);
    out[2 * n] = '\0';
    return mt_text(out);
}

/* Base64 in the standard alphabet, padded with '=', or url's, RFC 4648
   section 5, which spells 62 and 63 as '-' and '_' and drops the padding. */
static inline mt_atom *base64(const unsigned char *bytes, size_t n, bool url)
{
    unsigned char out[4 * (BASE64_MOST / 3 + 1) + 1];
    require("room for the base64", n <= BASE64_MOST);
    int len = EVP_EncodeBlock(out, bytes, (int)n);
    for (int i = 0; url && i < len; i++) out[i] = out[i] == '+' ? '-' : out[i] == '/' ? '_' : out[i];
    while (url && len && out[len - 1] == '=') len--;
    return mt_textn((const char *)out, (size_t)len);
}

/* The bytes base64 spells, the padding optional, as EVP_DecodeBlock reads a
   whole number of quads and counts the padding among its bytes: -1 when
   libcrypto refuses the text, or url text holds the standard '+', '/' or
   '='. */
static inline long unbase64(const char *text, size_t n, bool url, unsigned char *out)
{
    char padded[4 * (BASE64_MOST / 3 + 1)];
    if (n + 3 >= sizeof padded) return -1;
    for (size_t i = 0; i < n; i++) {
        char c = text[i];
        if (url && (c == '+' || c == '/' || c == '=')) return -1;
        padded[i] = url && c == '-' ? '+' : url && c == '_' ? '/' : c;
    }
    size_t whole = (n + 3) / 4 * 4, pads = 0;
    memset(padded + n, '=', whole - n);
    while (pads < whole && padded[whole - 1 - pads] == '=') pads++;
    int decoded = EVP_DecodeBlock(out, (const unsigned char *)padded, (int)whole);
    return decoded < 0 ? -1 : decoded - (long)pads;
}

static inline mt_atom *digest(const char *algorithm, const void *data, size_t n)
{
    unsigned char md[DIGEST_MOST];
    size_t length;
    require("EVP_Q_digest", EVP_Q_digest(NULL, algorithm, NULL, data, n, md, &length));
    return hex(md, length);
}

#endif
