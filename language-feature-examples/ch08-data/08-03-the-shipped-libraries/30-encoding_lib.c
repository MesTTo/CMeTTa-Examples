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
 * Assumes: libcrypto and libutf8proc, found through pkg-config.
 * Guarantees: all forty-three claims of the original hold [tested: make
 *   twins; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"
#include <ctype.h>
#include <utf8proc.h>
#include "crypto_oracle.h"

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
    metta *m = open_engine();
    require("import lib_encoding", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_encoding")))));
    require("import lib_string", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_string")))));

    /* UTF-8. */
    mt_atom *hi = utf8_bytes("hi"), *none = mt_unit();
    check_answers("utf8-encode", mt_eval(m, E("utf8-encode", T("hi"))), utf8_bytes("hi"));
    check_answers("utf8-decode", mt_eval(m, E("utf8-decode", mt_keep(hi))), utf8_text(hi));
    check_answers("no text, no bytes", mt_eval(m, E("utf8-encode", T(""))), utf8_bytes(""));
    check_answers("no bytes, no text", mt_eval(m, E("utf8-decode", mt_unit())), utf8_text(none));
    check_answers("an accent is two bytes", mt_eval(m, E("utf8-encode", T("é"))), utf8_bytes("é"));
    check_answers("and one character", mt_eval(m, E("string-length", T("é"))), (int64_t)codepoints("é"));
    check_answers("counted in bytes", mt_eval(m, E("size-atom", E("utf8-encode", T("é")))), (int64_t)strlen("é"));
    check_answers("an emoji is four", mt_eval(m, E("size-atom", E("utf8-encode", T("🙂")))), (int64_t)strlen("🙂"));
    check_answers("and one character", mt_eval(m, E("string-length", T("🙂"))), (int64_t)codepoints("🙂"));
    mt_atom *mixed = utf8_bytes("héllo 🙂");
    check_answers("a round trip", mt_eval(m, E("utf8-decode", E("utf8-encode", T("héllo 🙂")))), utf8_text(mixed));

    /* Hex. */
    mt_atom *ff = E(255, 16, 0);
    check_answers("hex-encode", mt_eval(m, E("hex-encode", mt_keep(ff))), hex_of(ff));
    check_answers("hex-decode", mt_eval(m, E("hex-decode", T("ff1000"))), unhex("ff1000"));
    check_answers("either case", mt_eval(m, E("hex-decode", T("FF1000"))), unhex("FF1000"));
    check_answers("no bytes, no hex", mt_eval(m, E("hex-encode", mt_unit())), hex_of(none));
    check_answers("no hex, no bytes", mt_eval(m, E("hex-decode", T(""))), unhex(""));
    check_answers("the hex of a text's bytes", mt_eval(m, E("hex-encode", E("utf8-encode", T("hi")))), hex_of(hi));

    /* Base64 in two alphabets. */
    mt_atom *high = E(255, 254);
    check_answers("standard base64", mt_eval(m, E("base64-encode", "standard", mt_keep(hi))), base64_of("standard", hi));
    check_answers("and back", mt_eval(m, E("base64-decode", "standard", T("aGk="))), unbase64_of("standard", "aGk="));
    check_answers("high bits, standard", mt_eval(m, E("base64-encode", "standard", mt_keep(high))), base64_of("standard", high));
    check_answers("high bits, url", mt_eval(m, E("base64-encode", "url", mt_keep(high))), base64_of("url", high));
    mt_atom *url_high = base64_of("url", high);
    check_answers("url round trip", mt_eval(m, E("base64-decode", "url", E("base64-encode", "url", mt_keep(high)))),
                  unbase64_of("url", mt_name(url_high)));
    check_answers("no bytes, no base64", mt_eval(m, E("base64-encode", "standard", mt_unit())), base64_of("standard", none));
    check_answers("no base64, no bytes", mt_eval(m, E("base64-decode", "standard", T(""))), unbase64_of("standard", ""));
    check_answers("three bytes, standard", mt_eval(m, E("base64-decode", "standard", T("//79"))), unbase64_of("standard", "//79"));
    check_answers("the same three, url", mt_eval(m, E("base64-decode", "url", T("__79"))), unbase64_of("url", "__79"));

    /* The encodings compose. */
    mt_atom *hello = utf8_bytes("héllo"), *hello64 = base64_of("url", hello), *hello_back = unbase64_of("url", mt_name(hello64));
    check_answers("text through a URL and back",
                  mt_eval(m, E("utf8-decode", E("base64-decode", "url", E("base64-encode", "url", E("utf8-encode", T("héllo")))))), utf8_text(hello_back));
    mt_atom *from_hex = unhex("6869");
    check_answers("hex into base64", mt_eval(m, E("base64-encode", "standard", E("hex-decode", T("6869")))), base64_of("standard", from_hex));

    /* Refusals. */
    static const char *const hexes[][2] = { { "hex of odd length", "abc" }, { "no hex digits", "zz" } };
    for (size_t i = 0; i < 2; i++)
        check_answers(hexes[i][0], mt_eval(m, guarded(E("hex-decode", T(hexes[i][1])))), verdict(computed(unhex(hexes[i][1]))));
    mt_atom *lone = E(255), *half = E(195);
    check_answers("no UTF-8 starts with 255", mt_eval(m, guarded(E("utf8-decode", mt_keep(lone)))), verdict(computed(utf8_text(lone))));
    check_answers("half a character", mt_eval(m, guarded(E("utf8-decode", mt_keep(half)))), verdict(computed(utf8_text(half))));
    check_answers("base64 libcrypto will not decode", mt_eval(m, guarded(E("base64-decode", "standard", T("not base64!")))),
                  verdict(computed(unbase64_of("standard", "not base64!"))));
    mt_atom *one = E(1);
    check_answers("an unknown alphabet", mt_eval(m, guarded(E("base64-encode", "nosuch", mt_keep(one)))), verdict(computed(base64_of("nosuch", one))));
    mt_atom *wide = E(256), *smile = codes("🙂"), *e_acute = codes("é"), *utf8_e = utf8_bytes("é");
    check_answers("a code point above 255 is no byte", mt_eval(m, guarded(E("hex-encode", mt_keep(wide)))), verdict(computed(hex_of(wide))));
    check_answers("so an emoji's code point", mt_eval(m, guarded(E("base64-encode", "standard", E("string-codes", T("🙂"))))),
                  verdict(computed(base64_of("standard", smile))));
    check_answers("a code point that fits is a byte", mt_eval(m, E("hex-encode", E("string-codes", T("é")))), hex_of(e_acute));
    check_answers("which is not the UTF-8", mt_eval(m, E("hex-encode", E("utf8-encode", T("é")))), hex_of(utf8_e));
    mt_atom *symbol_byte = E(1, "two"), *seven = mt_num(7);
    check_answers("a symbol is no byte", mt_eval(m, guarded(E("utf8-decode", mt_keep(symbol_byte)))), verdict(computed(utf8_text(symbol_byte))));
    check_answers("a number is no hex", mt_eval(m, guarded(E("hex-decode", mt_keep(seven)))), verdict(mt_kind_of(seven) == MT_TEXT));

    /* The hex equation read back out of &self, and alternatives. */
    mt_atom *lambda = NULL;
    mt_rows (row, mt_match(m, E("=", E("hex-encode", V("bytes")), V("body")))) {
        mt_drop(lambda);
        lambda = E("|->", E(mt_keep(mt_bound(row, "bytes"))), mt_keep(mt_bound(row, "body")));
    }
    require("hex-encode is an equation", lambda != NULL);
    mt_atom *format = mt_one(mt_eval(m, lambda)), *edge = E(0, 255);
    require("which evaluates", format != NULL);
    check_answers("the recipe applied", mt_eval(m, E(format, mt_keep(edge))), hex_of(edge));
    check_answers("one answer per alternative", mt_eval(m, E("collapse", E("hex-encode", E("superpose", E(mt_unit(), mt_keep(edge)))))),
                  E(hex_of(none), hex_of(edge)));

    /* A NUL is a byte like any other, and a digit outside ASCII is no hex. */
    static const utf8proc_int32_t with_nul[] = { 97, 0, 98 };
    unsigned char encoded[16];
    size_t used = 0;
    for (size_t i = 0; i < 3; i++) used += (size_t)utf8proc_encode_char(with_nul[i], encoded + used);
    mt_atom *nul_codes = E(97, 0, 98);
    check_answers("a NUL survives UTF-8", mt_eval(m, E("hex-encode", E("utf8-encode", E("string-from-codes", mt_keep(nul_codes))))),
                  hex(encoded, used));
    check_answers("a Unicode digit is no hex digit", mt_eval(m, guarded(E("hex-decode", T("𝟢0")))), verdict(computed(unhex("𝟢0"))));
    mt_drop(nul_codes);

    mt_atom *held[] = { hi, none, mixed, ff, high, url_high, hello, hello64, hello_back, from_hex, lone, half, one, wide, smile,
                        e_acute, utf8_e, symbol_byte, seven, format, edge };
    for (size_t i = 0; i < sizeof held / sizeof *held; i++) mt_drop(held[i]);
    return done(m);
}
