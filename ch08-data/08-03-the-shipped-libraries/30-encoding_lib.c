/* Purpose: lib_encoding, held against C's own encodings of the same bytes.
 *   Bytes are an expression of numbers from 0 to 255 in the engine and an
 *   unsigned char array in C, and a list holding anything else is no bytes.
 *   UTF-8 is a text's own bytes one way and utf8proc_iterate over them the
 *   other, which refuses a sequence that is not UTF-8; hex is two lowercase
 *   digits a byte, read back in either case; base64 is libcrypto's
 *   EVP_EncodeBlock and EVP_DecodeBlock in the standard alphabet or url's,
 *   from crypto_oracle.h, which 06-crypto_lib shares. C refuses what the
 *   library refuses: a byte above 255, hex of odd length or with a letter
 *   past f, bytes that are not UTF-8, base64 libcrypto will not decode and
 *   an alphabet it has no name for.
 * Build: cc 30-encoding_lib.c $(pkg-config --cflags --libs cmetta libcrypto
 *   libutf8proc)
 * Assumes: libcrypto and libutf8proc, found through pkg-config.
 * Guarantees: all forty-three claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#if __has_include(<openssl/evp.h>) && __has_include(<utf8proc.h>)
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <utf8proc.h>
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

typedef struct bytes {
    unsigned char at[MOST];
    size_t n;
} bytes;

/* The bytes a list holds; false when an element is no number from 0 to 255. */
static bool bytes_of(const mt_atom *list, bytes *out)
{
    out->n = 0;
    if (mt_kind_of(list) != MT_EXPR || mt_len(list) > MOST) return false;
    for (size_t i = 0; i < mt_len(list); i++) {
        const mt_atom *b = mt_at(list, i);
        if (mt_kind_of(b) != MT_INT || mt_int(b) < 0 || mt_int(b) > 255) return false;
        out->at[out->n++] = (unsigned char)mt_int(b);
    }
    return true;
}

/* UTF-8: a text is its bytes; bytes are text only when every sequence
   decodes. NULL when they do not, or are no bytes. */
static mt_atom *utf8_bytes(const char *text) { return mt_array(strlen(text), (const unsigned char *)text); }

static mt_atom *utf8_text(const mt_atom *list)
{
    bytes b;
    if (!bytes_of(list, &b)) return NULL;
    utf8proc_int32_t c;
    for (size_t i = 0; i < b.n;) {
        utf8proc_ssize_t used = utf8proc_iterate(b.at + i, (utf8proc_ssize_t)(b.n - i), &c);
        if (used <= 0) return NULL;
        i += (size_t)used;
    }
    return mt_textn((const char *)b.at, b.n);
}

static size_t codepoints(const char *text)
{
    size_t n = 0, len = strlen(text);
    utf8proc_int32_t c;
    for (utf8proc_ssize_t used; len; text += used, len -= (size_t)used, n++) {
        used = utf8proc_iterate((const utf8proc_uint8_t *)text, (utf8proc_ssize_t)len, &c);
        require("well-formed UTF-8", used > 0);
    }
    return n;
}

/* Hex: NULL for what is no bytes. */
static mt_atom *hex_of(const mt_atom *list)
{
    bytes b;
    return bytes_of(list, &b) ? hex(b.at, b.n) : NULL;
}

/* The bytes hex spells, in either case; NULL for a character that is no
   ASCII hex digit, which any byte of a longer UTF-8 character is not, or an
   odd count of digits. */
static mt_atom *unhex(const char *text)
{
    size_t len = strlen(text);
    unsigned char out[MOST];
    for (size_t i = 0; i < len; i++)
        if (!isxdigit((unsigned char)text[i])) return NULL;
    if (len % 2 || len / 2 > MOST) return NULL;
    for (size_t i = 0; i < len; i += 2) {
        char pair[3] = { text[i], text[i + 1], '\0' };
        out[i / 2] = (unsigned char)strtol(pair, NULL, 16);
    }
    return mt_array(len / 2, out);
}

/* The alphabets by name; -1 for a name the library does not know. */
static int alphabet(const char *name) { return strcmp(name, "standard") == 0 ? 0 : strcmp(name, "url") == 0 ? 1 : -1; }

static mt_atom *base64_of(const char *name, const mt_atom *list)
{
    bytes b;
    int url = alphabet(name);
    return url >= 0 && bytes_of(list, &b) ? base64(b.at, b.n, url) : NULL;
}

static mt_atom *unbase64_of(const char *name, const char *text)
{
    unsigned char out[MOST];
    int url = alphabet(name);
    long n = url >= 0 ? unbase64(text, strlen(text), url, out) : -1;
    return n >= 0 ? mt_array((size_t)n, out) : NULL;
}

static mt_atom *verdict(bool holds) { return S(holds ? "fine" : "refused"); }
static mt_atom *guarded(mt_atom *goal) { return E("if-error", E("catch", goal), "refused", "fine"); }

static bool computed(mt_atom *value)
{
    mt_drop(value);
    return value != NULL;
}

/* The code points of a text, as string-codes answers them. */
static mt_atom *codes(const char *text)
{
    mt_atom *out[MOST];
    size_t n = 0, len = strlen(text);
    utf8proc_int32_t c;
    for (utf8proc_ssize_t used; len; text += used, len -= (size_t)used) {
        used = utf8proc_iterate((const utf8proc_uint8_t *)text, (utf8proc_ssize_t)len, &c);
        require("well-formed UTF-8", used > 0);
        out[n++] = mt_num(c);
    }
    return mt_exprv(n, out);
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_encoding", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_encoding")))));
    require("import lib_string", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_string")))));

    /* UTF-8. */
    mt_atom *hi = utf8_bytes("hi"), *none = mt_unit();
    assert(answers_are(mt_eval(m, E("utf8-encode", T("hi"))), E(utf8_bytes("hi"))) && "utf8-encode");
    assert(answers_are(mt_eval(m, E("utf8-decode", mt_keep(hi))), E(utf8_text(hi))) && "utf8-decode");
    assert(answers_are(mt_eval(m, E("utf8-encode", T(""))), E(utf8_bytes(""))) && "no text, no bytes");
    assert(answers_are(mt_eval(m, E("utf8-decode", mt_unit())), E(utf8_text(none))) && "no bytes, no text");
    assert(answers_are(mt_eval(m, E("utf8-encode", T("é"))), E(utf8_bytes("é"))) && "an accent is two bytes");
    assert(answers_are(mt_eval(m, E("string-length", T("é"))), E((int64_t)codepoints("é"))) && "and one character");
    assert(answers_are(mt_eval(m, E("size-atom", E("utf8-encode", T("é")))), E((int64_t)strlen("é"))) && "counted in bytes");
    assert(answers_are(mt_eval(m, E("size-atom", E("utf8-encode", T("🙂")))), E((int64_t)strlen("🙂"))) && "an emoji is four");
    assert(answers_are(mt_eval(m, E("string-length", T("🙂"))), E((int64_t)codepoints("🙂"))) && "and one character");
    mt_atom *mixed = utf8_bytes("héllo 🙂");
    assert(answers_are(mt_eval(m, E("utf8-decode", E("utf8-encode", T("héllo 🙂")))), E(utf8_text(mixed))) && "a round trip");

    /* Hex. */
    mt_atom *ff = E(255, 16, 0);
    assert(answers_are(mt_eval(m, E("hex-encode", mt_keep(ff))), E(hex_of(ff))) && "hex-encode");
    assert(answers_are(mt_eval(m, E("hex-decode", T("ff1000"))), E(unhex("ff1000"))) && "hex-decode");
    assert(answers_are(mt_eval(m, E("hex-decode", T("FF1000"))), E(unhex("FF1000"))) && "either case");
    assert(answers_are(mt_eval(m, E("hex-encode", mt_unit())), E(hex_of(none))) && "no bytes, no hex");
    assert(answers_are(mt_eval(m, E("hex-decode", T(""))), E(unhex(""))) && "no hex, no bytes");
    assert(answers_are(mt_eval(m, E("hex-encode", E("utf8-encode", T("hi")))), E(hex_of(hi))) && "the hex of a text's bytes");

    /* Base64 in two alphabets. */
    mt_atom *high = E(255, 254);
    assert(answers_are(mt_eval(m, E("base64-encode", "standard", mt_keep(hi))), E(base64_of("standard", hi))) && "standard base64");
    assert(answers_are(mt_eval(m, E("base64-decode", "standard", T("aGk="))), E(unbase64_of("standard", "aGk="))) && "and back");
    assert(answers_are(mt_eval(m, E("base64-encode", "standard", mt_keep(high))), E(base64_of("standard", high))) && "high bits, standard");
    assert(answers_are(mt_eval(m, E("base64-encode", "url", mt_keep(high))), E(base64_of("url", high))) && "high bits, url");
    mt_atom *url_high = base64_of("url", high);
    assert(answers_are(mt_eval(m, E("base64-decode", "url", E("base64-encode", "url", mt_keep(high)))), E(unbase64_of("url", mt_name(url_high))))
           && "url round trip");
    assert(answers_are(mt_eval(m, E("base64-encode", "standard", mt_unit())), E(base64_of("standard", none))) && "no bytes, no base64");
    assert(answers_are(mt_eval(m, E("base64-decode", "standard", T(""))), E(unbase64_of("standard", ""))) && "no base64, no bytes");
    assert(answers_are(mt_eval(m, E("base64-decode", "standard", T("//79"))), E(unbase64_of("standard", "//79"))) && "three bytes, standard");
    assert(answers_are(mt_eval(m, E("base64-decode", "url", T("__79"))), E(unbase64_of("url", "__79"))) && "the same three, url");

    /* The encodings compose. */
    mt_atom *hello = utf8_bytes("héllo"), *hello64 = base64_of("url", hello), *hello_back = unbase64_of("url", mt_name(hello64));
    assert(answers_are(mt_eval(m, E("utf8-decode", E("base64-decode", "url", E("base64-encode", "url", E("utf8-encode", T("héllo")))))), E(utf8_text(hello_back)))
           && "text through a URL and back");
    mt_atom *from_hex = unhex("6869");
    assert(answers_are(mt_eval(m, E("base64-encode", "standard", E("hex-decode", T("6869")))), E(base64_of("standard", from_hex))) && "hex into base64");

    /* Refusals. */
    static const char *const hexes[][2] = { { "hex of odd length", "abc" }, { "no hex digits", "zz" } };
    for (size_t i = 0; i < 2; i++)
        assert(answers_are(mt_eval(m, guarded(E("hex-decode", T(hexes[i][1])))), E(verdict(computed(unhex(hexes[i][1]))))) && hexes[i][0]);
    mt_atom *lone = E(255), *half = E(195);
    assert(answers_are(mt_eval(m, guarded(E("utf8-decode", mt_keep(lone)))), E(verdict(computed(utf8_text(lone))))) && "no UTF-8 starts with 255");
    assert(answers_are(mt_eval(m, guarded(E("utf8-decode", mt_keep(half)))), E(verdict(computed(utf8_text(half))))) && "half a character");
    assert(answers_are(mt_eval(m, guarded(E("base64-decode", "standard", T("not base64!")))), E(verdict(computed(unbase64_of("standard", "not base64!")))))
           && "base64 libcrypto will not decode");
    mt_atom *one = E(1);
    assert(answers_are(mt_eval(m, guarded(E("base64-encode", "nosuch", mt_keep(one)))), E(verdict(computed(base64_of("nosuch", one))))) && "an unknown alphabet");
    mt_atom *wide = E(256), *smile = codes("🙂"), *e_acute = codes("é"), *utf8_e = utf8_bytes("é");
    assert(answers_are(mt_eval(m, guarded(E("hex-encode", mt_keep(wide)))), E(verdict(computed(hex_of(wide))))) && "a code point above 255 is no byte");
    assert(answers_are(mt_eval(m, guarded(E("base64-encode", "standard", E("string-codes", T("🙂"))))), E(verdict(computed(base64_of("standard", smile)))))
           && "so an emoji's code point");
    assert(answers_are(mt_eval(m, E("hex-encode", E("string-codes", T("é")))), E(hex_of(e_acute))) && "a code point that fits is a byte");
    assert(answers_are(mt_eval(m, E("hex-encode", E("utf8-encode", T("é")))), E(hex_of(utf8_e))) && "which is not the UTF-8");
    mt_atom *symbol_byte = E(1, "two"), *seven = mt_num(7);
    assert(answers_are(mt_eval(m, guarded(E("utf8-decode", mt_keep(symbol_byte)))), E(verdict(computed(utf8_text(symbol_byte))))) && "a symbol is no byte");
    assert(answers_are(mt_eval(m, guarded(E("hex-decode", mt_keep(seven)))), E(verdict(mt_kind_of(seven) == MT_TEXT))) && "a number is no hex");

    /* The hex equation read back out of &self, and alternatives. */
    mt_atom *lambda = NULL;
    mt_rows (row, mt_match(m, E("=", E("hex-encode", V("bytes")), V("body")))) {
        mt_drop(lambda);
        lambda = E("|->", E(mt_keep(mt_bound(row, "bytes"))), mt_keep(mt_bound(row, "body")));
    }
    require("hex-encode is an equation", lambda != NULL);
    mt_atom *format = mt_one(mt_eval(m, lambda)), *edge = E(0, 255);
    require("which evaluates", format != NULL);
    assert(answers_are(mt_eval(m, E(format, mt_keep(edge))), E(hex_of(edge))) && "the recipe applied");
    assert(answers_are(mt_eval(m, E("collapse", E("hex-encode", E("superpose", E(mt_unit(), mt_keep(edge)))))), E(E(hex_of(none), hex_of(edge))))
           && "one answer per alternative");

    /* A NUL is a byte like any other, and a digit outside ASCII is no hex. */
    static const utf8proc_int32_t with_nul[] = { 97, 0, 98 };
    unsigned char encoded[16];
    size_t used = 0;
    for (size_t i = 0; i < 3; i++) used += (size_t)utf8proc_encode_char(with_nul[i], encoded + used);
    mt_atom *nul_codes = E(97, 0, 98);
    assert(answers_are(mt_eval(m, E("hex-encode", E("utf8-encode", E("string-from-codes", mt_keep(nul_codes))))), E(hex(encoded, used)))
           && "a NUL survives UTF-8");
    assert(answers_are(mt_eval(m, guarded(E("hex-decode", T("𝟢0")))), E(verdict(computed(unhex("𝟢0"))))) && "a Unicode digit is no hex digit");
    mt_drop(nul_codes);

    mt_atom *held[] = { hi, none, mixed, ff, high, url_high, hello, hello64, hello_back, from_hex, lone, half, one, wide, smile,
                        e_acute, utf8_e, symbol_byte, seven, format, edge };
    for (size_t i = 0; i < sizeof held / sizeof *held; i++) mt_drop(held[i]);
    mt_close(m);
    return 0;
}
#else
#include <stdio.h>

/* Without OpenSSL's libcrypto and utf8proc's headers the program only says what it needs. */
int main(void)
{
    fputs("30-encoding_lib.c needs OpenSSL's libcrypto and utf8proc: install its development files, then build with\n"
          "cc 30-encoding_lib.c $(pkg-config --cflags --libs cmetta libcrypto libutf8proc)\n", stderr);
    return 77;
}
#endif
