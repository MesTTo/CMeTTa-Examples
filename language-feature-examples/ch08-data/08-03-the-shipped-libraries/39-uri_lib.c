/* Purpose: lib_uri, held against uriparser, C's RFC 3986 library, where the
 *   library does what RFC 3986 says: the strict grammar every reference must
 *   meet, and reference resolution by section 5.2. The components are split
 *   by the regular expression RFC 3986 appendix B gives, whose optional
 *   groups tell an absent component from an empty one. Normalization is
 *   section 6.2.2 written out in C, with dot segments removed as section
 *   5.2.4 says from anchored paths only and a path that would start with two
 *   slashes and no authority spelled /.// as the WHATWG URL standard spells
 *   it; uriparser's own normalizer answers foo://b there, a different URI,
 *   and removes a relative path's dots, which the library keeps. Encoding
 *   uses the character classes SWI-Prolog's uri library names for each
 *   context, decoding checks every escape and every UTF-8 sequence strictly
 *   through utf8proc, and query pairs split and join as the library says.
 * Guarantees: all fifty-five claims of the original hold [tested: make
 *   twins; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"
#include <ctype.h>
#include <regex.h>
#include <uriparser/Uri.h>
#include <utf8proc.h>

/* A growable byte string. */
typedef struct text {
    char *at;
    size_t n, cap;
} text;

static void put(text *t, const char *bytes, size_t n)
{
    if (t->n + n + 1 > t->cap) {
        t->cap = 2 * (t->n + n + 1);
        t->at = realloc(t->at, t->cap);
        require("room for the text", t->at != NULL);
    }
    memcpy(t->at + t->n, bytes, n);
    t->n += n;
    t->at[t->n] = '\0';
}

static void puts_(text *t, const char *s) { put(t, s, strlen(s)); }

static mt_atom *take(text *t)
{
    mt_atom *out = mt_textn(t->at ? t->at : "", t->n);
    free(t->at);
    *t = (text){ 0 };
    return out;
}

/* The five components, and whether each appeared. */
enum { SCHEME, AUTHORITY, PATH, QUERY, FRAGMENT, PARTS };
static const char *const part_names[PARTS] = { "scheme", "authority", "path", "query", "fragment" };

typedef struct reference {
    bool present[PARTS];
    char *part[PARTS];
} reference;

/* A named text: a component, or a query pair. Lengths are kept, since a
   query's text may hold NUL. */
typedef struct pair {
    const char *key, *value;
    size_t key_n, value_n;
} pair;
#define PAIR(key, value) { key, value, sizeof key - 1, sizeof value - 1 }

static void forget(reference *r)
{
    for (int i = 0; i < PARTS; i++) free(r->part[i]);
    *r = (reference){ 0 };
}

/* RFC 3986 appendix B: groups 2, 4, 5, 7 and 9 are the components, and
   groups 1, 3, 6 and 8 whether each optional one appeared at all. */
static regex_t appendix_b;
static const int group_of[PARTS] = { 2, 4, 5, 7, 9 }, presence_of[PARTS] = { 1, 3, 0, 6, 8 };

static void split(const char *s, reference *r)
{
    regmatch_t m[10];
    require("appendix B matches every string", regexec(&appendix_b, s, 10, m, 0) == 0);
    for (int i = 0; i < PARTS; i++) {
        r->present[i] = m[presence_of[i]].rm_so != -1;
        r->part[i] = r->present[i] ? strndup(s + m[group_of[i]].rm_so, (size_t)(m[group_of[i]].rm_eo - m[group_of[i]].rm_so)) : NULL;
    }
}

/* Whether the reference meets RFC 3986's grammar, NUL bytes included. */
static bool valid(const char *s, size_t n)
{
    UriUriA u;
    const char *error;
    if (uriParseSingleUriExA(&u, s, s + n, &error) != URI_SUCCESS) return false;
    uriFreeUriMembersA(&u);
    return true;
}

static mt_atom *rows(const reference *r)
{
    mt_atom *kids[PARTS];
    size_t n = 0;
    for (int i = 0; i < PARTS; i++)
        if (r->present[i]) kids[n++] = E(T(part_names[i]), T(r->part[i]));
    return mt_exprv(n, kids);
}

/* A reference's components; NULL when it breaks the grammar. */
static mt_atom *parts(const char *s, size_t n)
{
    if (!valid(s, n)) return NULL;
    reference r;
    split(s, &r);
    mt_atom *out = rows(&r);
    forget(&r);
    return out;
}

/* Section 5.3's recomposition, each present component with its delimiter. */
static void compose(const reference *r, text *out)
{
    static const char *const before[PARTS] = { "", "//", "", "?", "#" }, *const after[PARTS] = { ":", "", "", "", "" };
    for (int i = 0; i < PARTS; i++)
        if (r->present[i]) puts_(out, before[i]), puts_(out, r->part[i]), puts_(out, after[i]);
}

/* A reference from named components in any order, the path empty when
   missing; NULL for an unknown or repeated name, or components whose
   delimiters would read back as different ones. */
static mt_atom *built(const pair *given, size_t n)
{
    reference r = { 0 }, again;
    for (size_t i = 0; i < n; i++) {
        int k = 0;
        while (k < PARTS && strcmp(part_names[k], given[i].key) != 0) k++;
        /* No component's grammar has a NUL in it. */
        if (k == PARTS || r.present[k] || memchr(given[i].value, 0, given[i].value_n)) return forget(&r), NULL;
        r.present[k] = true;
        r.part[k] = strdup(given[i].value);
    }
    if (!r.present[PATH]) r.present[PATH] = true, r.part[PATH] = strdup("");
    text out = { 0 };
    compose(&r, &out);
    put(&out, "", 0);
    bool fine = valid(out.at, out.n);
    if (fine) {
        split(out.at, &again);
        for (int i = 0; i < PARTS; i++)
            fine &= again.present[i] == r.present[i] && (!r.present[i] || strcmp(again.part[i], r.part[i]) == 0);
        forget(&again);
    }
    forget(&r);
    mt_atom *atom = take(&out);
    if (!fine) mt_drop(atom), atom = NULL;
    return atom;
}

static bool unreserved(unsigned char c) { return c && (isalnum(c) || strchr("-._~", c)); }

static int hex_digit(int c) { return isdigit(c) ? c - '0' : isxdigit(c) ? tolower(c) - 'a' + 10 : -1; }

/* Section 6.2.2.1 and 6.2.2.2: an escaped unreserved byte decodes, every
   other escape's digits are upper-cased. */
static char *percent_normalized(const char *s)
{
    static const char digits[] = "0123456789ABCDEF";
    text out = { 0 };
    put(&out, "", 0);
    for (; *s; s++)
        if (s[0] == '%' && hex_digit(s[1]) >= 0 && hex_digit(s[2]) >= 0) {
            unsigned char byte = (unsigned char)(hex_digit(s[1]) * 16 + hex_digit(s[2]));
            char escaped[3] = { '%', digits[byte >> 4], digits[byte & 15] };
            if (unreserved(byte)) put(&out, (char *)&byte, 1);
            else put(&out, escaped, 3);
            s += 2;
        } else
            put(&out, s, 1);
    return out.at;
}

/* Lowercase ASCII from `from`, stepping over escapes, whose digits are
   already upper-case. */
static void lower_from(char *s, size_t from)
{
    for (size_t i = from; s[i]; i++)
        if (s[i] == '%' && s[i + 1] && s[i + 2]) i += 2;
        else s[i] = (char)tolower((unsigned char)s[i]);
}

/* Section 5.2.4's remove_dot_segments, its five rules in order over the
   input, the output a buffer whose last segment rule C pops.
   Time: Theta(n) for an n-byte path; every byte moves at most once. */
static char *without_dots(const char *path)
{
    const char *in = path;
    char *out = malloc(strlen(path) + 1);
    size_t used = 0;
    require("room for the path", out != NULL);
    while (*in) {
        bool parent = false;
        if (strncmp(in, "../", 3) == 0) in += 3;
        else if (strncmp(in, "./", 2) == 0) in += 2;
        else if (strncmp(in, "/./", 3) == 0) in += 2;
        else if (strcmp(in, "/.") == 0) in = "/";
        else if (strncmp(in, "/../", 4) == 0) in += 3, parent = true;
        else if (strcmp(in, "/..") == 0) in = "/", parent = true;
        else if (strcmp(in, ".") == 0 || strcmp(in, "..") == 0) in += strlen(in);
        else {
            size_t k = (in[0] == '/') + strcspn(in + (in[0] == '/'), "/");
            memcpy(out + used, in, k);
            used += k;
            in += k;
        }
        if (parent) {
            while (used && out[used - 1] != '/') used--;
            if (used) used--;
        }
    }
    out[used] = '\0';
    return out;
}

/* Section 6.2.2's syntax-based normalization: the scheme and host
   lower-cased, the userinfo left as spelled, escapes normalized in every
   component, dots removed from a path anchored by a scheme, an authority or
   a leading slash, and a path that would start with two slashes and no
   authority spelled from /. so it reads back the same [source: RFC 3986,
   sections 3.3, 5.2.4 and 6.2.2; https://url.spec.whatwg.org/#url-serializing,
   step 3.2]. NULL for a reference that breaks the grammar. */
static mt_atom *normalized(const char *s)
{
    if (!valid(s, strlen(s))) return NULL;
    reference r;
    split(s, &r);
    for (int i = AUTHORITY; i < PARTS; i++)
        if (r.present[i]) {
            char *n = percent_normalized(r.part[i]);
            free(r.part[i]);
            r.part[i] = n;
        }
    if (r.present[SCHEME]) lower_from(r.part[SCHEME], 0);
    if (r.present[AUTHORITY]) {
        char *at = strrchr(r.part[AUTHORITY], '@');
        lower_from(r.part[AUTHORITY], at ? (size_t)(at - r.part[AUTHORITY]) + 1 : 0);
    }
    if (r.present[SCHEME] || r.present[AUTHORITY] || r.part[PATH][0] == '/') {
        char *n = without_dots(r.part[PATH]);
        free(r.part[PATH]);
        r.part[PATH] = n;
    }
    if (!r.present[AUTHORITY] && strncmp(r.part[PATH], "//", 2) == 0) {
        char *n = malloc(strlen(r.part[PATH]) + 3);
        require("room for the path", n != NULL);
        strcat(strcpy(n, "/."), r.part[PATH]);
        free(r.part[PATH]);
        r.part[PATH] = n;
    }
    text out = { 0 };
    compose(&r, &out);
    forget(&r);
    return take(&out);
}

/* Section 5.2 through uriparser; NULL when the base has no scheme. */
static mt_atom *resolved(const char *reference_text, const char *base_text)
{
    UriUriA ref, base, target;
    const char *error;
    mt_atom *out = NULL;
    if (uriParseSingleUriA(&ref, reference_text, &error) != URI_SUCCESS) return NULL;
    if (uriParseSingleUriA(&base, base_text, &error) != URI_SUCCESS) return uriFreeUriMembersA(&ref), NULL;
    if (uriAddBaseUriA(&target, &ref, &base) == URI_SUCCESS) {
        int chars = 0;
        uriToStringCharsRequiredA(&target, &chars);
        char *s = malloc((size_t)chars + 1);
        require("room for the URI", s != NULL);
        uriToStringA(s, &target, chars + 1, NULL);
        out = T(s);
        free(s);
        uriFreeUriMembersA(&target);
    }
    uriFreeUriMembersA(&ref);
    uriFreeUriMembersA(&base);
    return out;
}

/* The encoding contexts and the bytes each leaves as they are, beside
   unreserved ones [source: swipl-devel packages/clib/uri.c, ESC_PATH,
   ESC_SEGMENT, ESC_QVALUE and ESC_FRAGMENT, tag V10.1.14]. */
static const struct context {
    const char *name, *kept;
} contexts[] = {
    { "path", "!$&'()+*,;=/@" },
    { "segment", "!$&'()+*,;=@" },
    { "query-value", "!$'()*,/?@" },
    { "fragment", "!$&'()+*,;=:@/?" },
};
enum { CONTEXTS = sizeof contexts / sizeof *contexts };

static const struct context *context_named(const char *name)
{
    for (size_t i = 0; i < CONTEXTS; i++)
        if (strcmp(contexts[i].name, name) == 0) return &contexts[i];
    return NULL;
}

/* Text's UTF-8 bytes, each kept or escaped with upper-case digits. */
static void encode_into(const struct context *c, const char *s, size_t n, text *out)
{
    static const char digits[] = "0123456789ABCDEF";
    for (size_t i = 0; i < n; i++) {
        unsigned char byte = (unsigned char)s[i];
        char escaped[3] = { '%', digits[byte >> 4], digits[byte & 15] };
        if (unreserved(byte) || (byte && strchr(c->kept, byte))) put(out, (char *)&byte, 1);
        else put(out, escaped, 3);
    }
}

static mt_atom *encoded(const char *context, const char *s, size_t n)
{
    const struct context *c = context_named(context);
    if (!c) return NULL;
    text out = { 0 };
    put(&out, "", 0);
    encode_into(c, s, n, &out);
    return take(&out);
}

/* Every escape decoded exactly once, then the bytes read as strict UTF-8:
   no malformed escape, overlong form, surrogate or truncated sequence;
   false otherwise, the caller discarding what was written. */
static bool decode_into(const char *s, size_t n, text *out)
{
    size_t start = out->n;
    for (size_t i = 0; i < n; i++)
        if (s[i] == '%') {
            if (i + 2 >= n) return false;
            int hi = hex_digit(s[i + 1]), lo = hex_digit(s[i + 2]);
            if (hi < 0 || lo < 0) return false;
            char byte = (char)(hi * 16 + lo);
            put(out, &byte, 1);
            i += 2;
        } else
            put(out, s + i, 1);
    utf8proc_int32_t code;
    for (size_t i = start; i < out->n;) {
        utf8proc_ssize_t step = utf8proc_iterate((const utf8proc_uint8_t *)out->at + i, (utf8proc_ssize_t)(out->n - i), &code);
        if (step <= 0) return false;
        i += (size_t)step;
    }
    return true;
}

static mt_atom *decoded(const char *s)
{
    text out = { 0 };
    put(&out, "", 0);
    bool fine = decode_into(s, strlen(s), &out);
    mt_atom *atom = take(&out);
    if (!fine) mt_drop(atom), atom = NULL;
    return atom;
}

static bool known_style(const char *style) { return strcmp(style, "uri") == 0 || strcmp(style, "form") == 0; }

/* One key or value: form reads plus as space before it decodes. */
static mt_atom *query_text(bool form, const char *s, size_t n)
{
    char *plain = strndup(s, n);
    require("room for the pair", plain != NULL);
    if (form)
        for (char *p = plain; *p; p++)
            if (*p == '+') *p = ' ';
    text out = { 0 };
    put(&out, "", 0);
    bool fine = decode_into(plain, strlen(plain), &out);
    free(plain);
    mt_atom *atom = take(&out);
    if (!fine) mt_drop(atom), atom = NULL;
    return atom;
}

/* Ampersand-separated pairs in order, empty segments skipped, a bare key
   paired with an empty value; NULL for an unknown style or a bad escape. */
static mt_atom *query_pairs(const char *style, const char *query)
{
    if (!known_style(style)) return NULL;
    bool form = strcmp(style, "form") == 0;
    size_t count = 1;
    for (const char *p = query; *p; p++) count += *p == '&';
    mt_atom **kids = malloc(count * sizeof *kids);
    size_t n = 0;
    bool fine = true;
    require("room for the pairs", kids != NULL);
    for (const char *p = query; fine && *p;) {
        size_t len = strcspn(p, "&");
        if (len) {
            const char *eq = memchr(p, '=', len);
            size_t key_len = eq ? (size_t)(eq - p) : len;
            mt_atom *key = query_text(form, p, key_len), *value = query_text(form, eq ? eq + 1 : p + len, eq ? len - key_len - 1 : 0);
            fine = key && value;
            if (fine) kids[n++] = E(key, value);
            else mt_drop(key), mt_drop(value);
        }
        p += len + (p[len] == '&');
    }
    mt_atom *out = fine ? mt_exprv(n, kids) : NULL;
    if (!fine)
        for (size_t i = 0; i < n; i++) mt_drop(kids[i]);
    free(kids);
    return out;
}

/* Pairs joined as key=value with ampersands, each side encoded as a query
   value, form spelling a space as plus; NULL for an unknown style. */
static mt_atom *query_text_of(const char *style, const pair *pairs, size_t n)
{
    if (!known_style(style)) return NULL;
    bool form = strcmp(style, "form") == 0;
    text out = { 0 };
    put(&out, "", 0);
    for (size_t i = 0; i < n; i++) {
        text side = { 0 };
        if (i) put(&out, "&", 1);
        for (int k = 0; k < 2; k++) {
            put(&side, "", 0);
            if (k == 0) encode_into(context_named("query-value"), pairs[i].key, pairs[i].key_n, &side);
            else encode_into(context_named("query-value"), pairs[i].value, pairs[i].value_n, &side);
            for (size_t j = 0; j < side.n; j++)
                if (form && strncmp(side.at + j, "%20", 3) == 0) put(&out, "+", 1), j += 2;
                else put(&out, side.at + j, 1);
            if (k == 0) put(&out, "=", 1);
            free(side.at);
            side = (text){ 0 };
        }
    }
    return take(&out);
}

/* The values paired with a key, in order, into *out, which the caller
   frees. */
static size_t lookup(const mt_atom *pairs, const char *key, mt_atom ***out)
{
    size_t found = 0;
    *out = malloc((mt_len(pairs) + 1) * sizeof **out);
    require("room for the values", *out != NULL);
    for (size_t i = 0; i < mt_len(pairs); i++)
        if (strcmp(mt_name(mt_at(mt_at(pairs, i), 0)), key) == 0) (*out)[found++] = mt_keep(mt_at(mt_at(pairs, i), 1));
    return found;
}

/* The pairs a row expression holds, borrowing its texts. */
static pair *as_pairs(const mt_atom *rows)
{
    pair *pairs = malloc((mt_len(rows) + 1) * sizeof *pairs);
    require("room for the pairs", pairs != NULL);
    for (size_t i = 0; i < mt_len(rows); i++) {
        const mt_atom *key = mt_at(mt_at(rows, i), 0), *value = mt_at(mt_at(rows, i), 1);
        pairs[i] = (pair){ mt_name(key), mt_name(value), mt_name_len(key), mt_name_len(value) };
    }
    return pairs;
}

static mt_atom *known(mt_atom *value)
{
    require("the C value", value != NULL);
    return value;
}

static bool computed(mt_atom *value)
{
    mt_drop(value);
    return value != NULL;
}

static mt_atom *verdict(bool holds) { return S(holds ? "fine" : "refused"); }
static mt_answers *guarded(metta *m, mt_atom *goal) { return mt_eval(m, E("if-error", E("catch", goal), "refused", "fine")); }

static mt_atom *value_of(metta *m, mt_atom *goal)
{
    mt_atom *v = mt_one(mt_eval(m, goal));
    require("a value", v != NULL);
    return v;
}

/* Pairs as the engine takes them: rows of two texts. */
static mt_atom *given_rows(const pair *given, size_t n)
{
    mt_atom **kids = malloc((n + 1) * sizeof *kids);
    require("room for the rows", kids != NULL);
    for (size_t i = 0; i < n; i++) kids[i] = E(mt_textn(given[i].key, given[i].key_n), mt_textn(given[i].value, given[i].value_n));
    mt_atom *out = mt_exprv(n, kids);
    free(kids);
    return out;
}

int main(void)
{
    require("appendix B compiles", regcomp(&appendix_b, "^(([^:/?#]+):)?(//([^/?#]*))?([^?#]*)(\\?([^#]*))?(#(.*))?", REG_EXTENDED) == 0);
    metta *m = open_engine();
    static const char *const imports[] = { "lib_uri", "lib_pairs", "lib_encoding" };
    for (size_t i = 0; i < 3; i++) require(imports[i], mt_one_truth(mt_eval(m, E("import!", "&self", E("library", imports[i])))));

    /* Absent differs from empty, and every component keeps its escapes. */
    static const char *const splits[] = { "", "?#", "https://User:Pass@[::1]:0012/a%2Fb?q=1#F", "urn:Example:ABC" };
    for (size_t i = 0; i < 4; i++) check_answers(splits[i], mt_eval(m, E("uri-parts", T(splits[i]))), known(parts(splits[i], strlen(splits[i]))));
    static const pair query_and_fragment[] = { PAIR("query", ""), PAIR("fragment", "") },
                      out_of_order[] = { PAIR("path", "/x"), PAIR("authority", "host:"), PAIR("scheme", "http") };
    check_answers("nothing builds the empty reference", mt_eval(m, E("uri-build", mt_unit())), known(built(NULL, 0)));
    check_answers("empty but present", mt_eval(m, E("uri-build", given_rows(query_and_fragment, 2))), known(built(query_and_fragment, 2)));
    check_answers("any order, an empty port", mt_eval(m, E("uri-build", given_rows(out_of_order, 3))), known(built(out_of_order, 3)));
    mt_atom *split_parts = value_of(m, E("uri-parts", T("a:b?x#")));
    pair *round = as_pairs(split_parts);
    check_answers("parts build back", mt_eval(m, E("uri-build", mt_keep(split_parts))), known(built(round, mt_len(split_parts))));
    free(round);

    /* Normalization. */
    static const char *const normals[] = { "HTTP://User:Pass@HOST/a/%2e%2e/b?x=%7e#F", "urn:Example:ABC", "http://HOST/%ff/%c0%af/%2f?#",
                                           "../a/./b", "foo:/a/..//b" };
    for (size_t i = 0; i < 5; i++) check_answers(normals[i], mt_eval(m, E("uri-normalize", T(normals[i]))), known(normalized(normals[i])));

    /* Resolution. */
    static const char *const resolutions[][2] = {
        { "g", "http://a" },         { "../g", "http://a/b/c/d;p?q" },       { "?", "http://a/b?old#f" },
        { "#", "http://a/b?old#f" }, { "", "http://a/b?old#f" },             { "//other/x", "http://a/b" },
        { "http:g", "http://a/b" },  { "urn:Example:ABC", "http://a/b" },    { "g?y/../x", "http://a/b/c/d;p?q" },
        { "%2e%2e/g", "http://a/b/" },
    };
    for (size_t i = 0; i < 10; i++)
        check_answers(resolutions[i][0], mt_eval(m, E("uri-resolve", T(resolutions[i][0]), T(resolutions[i][1]))),
                      known(resolved(resolutions[i][0], resolutions[i][1])));

    /* Encoding in the component the data will live in. */
    mt_atom *names[CONTEXTS];
    for (size_t i = 0; i < CONTEXTS; i++) names[i] = S(contexts[i].name);
    check_answers("the contexts", mt_eval(m, E("uri-contexts")), mt_exprv(CONTEXTS, names));
    static const char *const encodings[][2] = { { "path", "a b/c?d" }, { "segment", "a b/c?d" }, { "query-value", "a+b&c=d" },
                                                { "fragment", "a+b&c=d/x?y" }, { "segment", "π🙂" } };
    for (size_t i = 0; i < 5; i++)
        check_answers(encodings[i][0], mt_eval(m, E("uri-encode", S(encodings[i][0]), T(encodings[i][1]))),
                      known(encoded(encodings[i][0], encodings[i][1], strlen(encodings[i][1]))));
    static const char nul[] = { 'a', '\0', 'b' };
    check_answers("NUL is %00", mt_eval(m, E("uri-encode", "segment", mt_textn(nul, 3))), known(encoded("segment", nul, 3)));
    check_answers("decoding", mt_eval(m, E("uri-decode", T("%CF%80%F0%9F%99%82"))), known(decoded("%CF%80%F0%9F%99%82")));
    check_answers("exactly once, plus kept", mt_eval(m, E("uri-decode", T("%252F+a"))), known(decoded("%252F+a")));
    check_answers("an encoded NUL survives", mt_eval(m, E("uri-decode", T("%00x"))), known(decoded("%00x")));

    /* Query pairs. */
    static const char *const messy = "a+b=c+d&&a=2&bare&empty=&";
    check_answers("a uri query", mt_eval(m, E("uri-query-parse", "uri", T(messy))), known(query_pairs("uri", messy)));
    check_answers("a form query", mt_eval(m, E("uri-query-parse", "form", T(messy))), known(query_pairs("form", messy)));
    check_answers("a semicolon is data", mt_eval(m, E("uri-query-parse", "uri", T("a=1;b=2&=x"))), known(query_pairs("uri", "a=1;b=2&=x")));
    check_answers("no query, no pairs", mt_eval(m, E("uri-query-parse", "form", T(""))), known(query_pairs("form", "")));
    static const pair three[] = { PAIR("a b", "c+d"), PAIR("a b", ""), PAIR("", "π🙂") };
    mt_atom *three_rows = given_rows(three, 3);
    check_answers("a uri query built", mt_eval(m, E("uri-query-build", "uri", mt_keep(three_rows))), known(query_text_of("uri", three, 3)));
    check_answers("a form query built", mt_eval(m, E("uri-query-build", "form", mt_keep(three_rows))), known(query_text_of("form", three, 3)));
    check_answers("no pairs, no query", mt_eval(m, E("uri-query-build", "form", mt_unit())), known(query_text_of("form", NULL, 0)));
    mt_atom *repeated = value_of(m, E("uri-query-parse", "uri", T("a=1&a=2&b=3"))), *mine = known(query_pairs("uri", "a=1&a=2&b=3")),
            **values;
    size_t n_values = lookup(mine, "a", &values);
    check_answers_("a repeated key's values", mt_eval(m, E("pairs-lookup", mt_keep(repeated), T("a"))), n_values, values);
    free(values);
    mt_atom *parsed = value_of(m, E("uri-query-parse", "form", T("%2B=%00&x=a/b?c"))), *mine_parsed = known(query_pairs("form", "%2B=%00&x=a/b?c"));
    pair *again = as_pairs(mine_parsed);
    check_answers("a form query round trip", mt_eval(m, E("uri-query-build", "form", mt_keep(parsed))),
                  known(query_text_of("form", again, mt_len(mine_parsed))));
    free(again);

    /* Refusals: invalid bytes and ambiguous boundaries. */
    static const pair ambiguous[] = { PAIR("path", "a"), PAIR("authority", "host") }, twice[] = { PAIR("path", "a"), PAIR("path", "b") },
                      unknown[] = { PAIR("missing", "x") };
    check_answers("a space", guarded(m, E("uri-parts", T("a b"))), verdict(computed(parts("a b", 3))));
    check_answers("a NUL", guarded(m, E("uri-parts", mt_textn(nul, 3))), verdict(computed(parts(nul, 3))));
    check_answers("a bad escape", guarded(m, E("uri-parts", T("x%ZZ"))), verdict(computed(parts("x%ZZ", 4))));
    check_answers("a path that would join its authority", guarded(m, E("uri-build", given_rows(ambiguous, 2))), verdict(computed(built(ambiguous, 2))));
    check_answers("a path twice", guarded(m, E("uri-build", given_rows(twice, 2))), verdict(computed(built(twice, 2))));
    check_answers("an unknown component", guarded(m, E("uri-build", given_rows(unknown, 1))), verdict(computed(built(unknown, 1))));
    check_answers("a relative base", guarded(m, E("uri-resolve", T("x"), T("relative"))), verdict(computed(resolved("x", "relative"))));
    check_answers("an unknown context", guarded(m, E("uri-encode", "missing", T("x"))), verdict(computed(encoded("missing", "x", 1))));
    static const char *const undecodable[] = { "%", "%FF", "%C0%AF" };
    for (size_t i = 0; i < 3; i++)
        check_answers(undecodable[i], guarded(m, E("uri-decode", T(undecodable[i]))), verdict(computed(decoded(undecodable[i]))));
    check_answers("a surrogate in a query", guarded(m, E("uri-query-parse", "form", T("x=%ED%A0%80"))), verdict(computed(query_pairs("form", "x=%ED%A0%80"))));
    check_answers("an unknown style", guarded(m, E("uri-query-build", "missing", mt_unit())), verdict(computed(query_text_of("missing", NULL, 0))));

    mt_atom *held[] = { split_parts, three_rows, repeated, mine, parsed, mine_parsed };
    for (size_t i = 0; i < sizeof held / sizeof *held; i++) mt_drop(held[i]);
    regfree(&appendix_b);
    return done(m);
}
