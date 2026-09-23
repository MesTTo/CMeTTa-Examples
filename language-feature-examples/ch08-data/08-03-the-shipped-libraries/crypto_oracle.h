/* Purpose: libcrypto as the oracle for lib_crypto's digests, shared by
 *   06-crypto_lib and 16-the_prolog_rung: EVP_Q_digest over the same bytes,
 *   spelled as the lowercase hex lib_crypto answers in. Static inline, so a
 *   twin that uses part of it compiles clean.
 * Assumes: libcrypto 3, and common.h included first with MT_SHORTHAND.
 */
#ifndef CRYPTO_ORACLE_H
#define CRYPTO_ORACLE_H
#include <openssl/crypto.h>
#include <openssl/evp.h>

enum { DIGEST_MOST = 64 };

/* Bytes as lowercase hex, the spelling lib_crypto answers in. */
static inline mt_atom *hex(const unsigned char *bytes, size_t n)
{
    char out[2 * DIGEST_MOST + 1];
    for (size_t i = 0; i < n; i++) snprintf(out + 2 * i, 3, "%02x", bytes[i]);
    out[2 * n] = '\0';
    return mt_text(out);
}

static inline mt_atom *digest(const char *algorithm, const void *data, size_t n)
{
    unsigned char md[DIGEST_MOST];
    size_t length;
    require("EVP_Q_digest", EVP_Q_digest(NULL, algorithm, NULL, data, n, md, &length));
    return hex(md, length);
}

#endif
