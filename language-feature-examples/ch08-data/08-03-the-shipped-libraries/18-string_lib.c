/* Purpose: lib_string, held against the same text operations written in C
 *   over codepoints, the unit every lib_string index and length counts.
 *   utf8proc, the C Unicode library, decodes and encodes UTF-8 and maps case;
 *   searching, counting, splitting, padding and lines are loops over the
 *   codepoint arrays. The metrics are the textbook ones: Levenshtein by
 *   Wagner-Fischer, similarity as one less the distance over the longer
 *   length, and ISub as SWI's C, which the library's native object runs.
 *   Wrapping follows SWI's paragraph filler, templates SWI's interpolation,
 *   and a number's text is read with strtoll under Prolog's radix prefixes.
 *   The library coerces Symbols and Numbers to text; C does it in text_of,
 *   giving an integer its decimal digits. Where the library refuses, C's
 *   own function refuses the same input, and the recipe the original applies
 *   is the equation C matches out of &self.
 * Assumes: libutf8proc, found through pkg-config.
 * Guarantees: all fifty claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include <ctype.h>
#include <inttypes.h>
#include <math.h>
#include <utf8proc.h>

enum { MOST = 96 };

/* A text as its codepoints. */
typedef struct codes {
    int32_t at[MOST];
    size_t n;
} codes;

static codes decoded(const char *text, size_t len)
{
    codes c = { .n = 0 };
    for (utf8proc_ssize_t used; len; text += used, len -= (size_t)used) {
        require("room for the codepoints", c.n < MOST);
        used = utf8proc_iterate((const utf8proc_uint8_t *)text, (utf8proc_ssize_t)len, &c.at[c.n++]);
        require("well-formed UTF-8", used > 0);
    }
    return c;
}

static codes of(const char *text) { return decoded(text, strlen(text)); }

static mt_atom *encoded(const int32_t *at, size_t n)
{
    utf8proc_uint8_t bytes[4 * MOST];
    size_t len = 0;
    for (size_t i = 0; i < n; i++) len += (size_t)utf8proc_encode_char(at[i], bytes + len);
    return mt_textn((const char *)bytes, len);
}

static void push(codes *out, const int32_t *at, size_t n)
{
    require("room for the text", out->n + n <= MOST);
    memcpy(out->at + out->n, at, n * sizeof *at);
    out->n += n;
}

/* A value as the library reads it as text: a String or a Symbol its name, an
   integer its decimal digits [source: lib/lib_string/lib.metta, "Text accepts
   Strings, Symbols and Numbers"; commit=33c2d50c84b24c1a2c906225600e2a4ffdb662f1].
   The digits live in one of four buffers, reused in turn. */
static const char *text_of(const mt_atom *value)
{
    static char digits[4][24];
    static size_t next;
    if (mt_kind_of(value) != MT_INT) return mt_name(value);
    char *out = digits[next++ % 4];
    snprintf(out, sizeof digits[0], "%" PRId64, mt_int(value));
    return out;
}

/* Searching. An empty part occurs at every boundary, both ends included. */
static bool here(const codes *t, size_t i, const codes *part)
{
    return i + part->n <= t->n && memcmp(t->at + i, part->at, part->n * sizeof *part->at) == 0;
}

static int64_t index_of(const char *text, const char *part)
{
    codes t = of(text), p = of(part);
    for (size_t i = 0; i <= t.n; i++)
        if (here(&t, i, &p)) return (int64_t)i;
    return -1;
}

static int64_t last_index_of(const char *text, const char *part)
{
    codes t = of(text), p = of(part);
    for (size_t i = t.n + 1; i-- > 0;)
        if (here(&t, i, &p)) return (int64_t)i;
    return -1;
}

static bool starts_with(const char *text, const char *part) { return index_of(text, part) == 0; }

static bool ends_with(const char *text, const char *part)
{
    codes t = of(text), p = of(part);
    return p.n <= t.n && here(&t, t.n - p.n, &p);
}

static int64_t count(const char *text, const char *part, bool overlap)
{
    codes t = of(text), p = of(part);
    int64_t n = 0;
    for (size_t i = 0; i <= t.n;)
        if (here(&t, i, &p)) n++, i += overlap || !p.n ? 1 : p.n;
        else i++;
    return n;
}

static mt_atom *replaced(const char *text, const char *from, const char *to)
{
    codes t = of(text), f = of(from), r = of(to), out = { .n = 0 };
    for (size_t i = 0; i < t.n;)
        if (f.n && here(&t, i, &f)) push(&out, r.at, r.n), i += f.n;
        else push(&out, &t.at[i++], 1);
    return encoded(out.at, out.n);
}

/* Splitting: a separator is one codepoint of a set, or one whole text, and
   each gives the length it covers at i, 0 where there is none. */
typedef struct span {
    size_t from, to;
} span;
typedef size_t separator(const codes *t, size_t i, const codes *s);

static size_t any_of(const codes *t, size_t i, const codes *s)
{
    for (size_t k = 0; k < s->n; k++)
        if (t->at[i] == s->at[k]) return 1;
    return 0;
}

static size_t exactly(const codes *t, size_t i, const codes *s) { return here(t, i, s) ? s->n : 0; }

static size_t split(const codes *t, separator *sep, const codes *s, span *out)
{
    size_t n = 0, start = 0;
    for (size_t i = 0, len;; i += len ? len : 1) {
        len = i < t->n ? sep(t, i, s) : 0;
        if (i < t->n && !len) continue;
        require("room for the pieces", n < MOST);
        out[n++] = (span){ start, i };
        if (i == t->n) return n;
        start = i + len;
    }
}

static mt_atom *pieces(const codes *t, const span *piece, size_t n)
{
    mt_atom *kids[MOST];
    for (size_t i = 0; i < n; i++) kids[i] = encoded(t->at + piece[i].from, piece[i].to - piece[i].from);
    return mt_exprv(n, kids);
}

static mt_atom *split_by(const char *separators, const char *text, separator *sep)
{
    codes s = of(separators), t = of(text);
    span piece[MOST];
    require("a nonempty exact separator", sep != exactly || s.n);
    return pieces(&t, piece, split(&t, sep, &s, piece));
}

/* LF-separated lines, one terminal empty one dropped. */
static size_t lines_of(const codes *t, span *line)
{
    codes lf = of("\n");
    size_t n = split(t, exactly, &lf, line);
    return line[n - 1].from == line[n - 1].to ? n - 1 : n;
}

static mt_atom *trimmed(const char *text)
{
    static const char space[] = " \t\n\r";
    size_t start = strspn(text, space), end = strlen(text);
    while (end > start && strchr(space, text[end - 1])) end--;
    return mt_textn(text + start, end - start);
}

static mt_atom *cased(const char *text, utf8proc_int32_t (*map)(utf8proc_int32_t))
{
    codes t = of(text);
    for (size_t i = 0; i < t.n; i++) t.at[i] = map(t.at[i]);
    return encoded(t.at, t.n);
}

static mt_atom *sliced(const char *text, int64_t from, int64_t to)
{
    codes t = of(text);
    size_t a = from < 0 ? 0 : (size_t)from > t.n ? t.n : (size_t)from;
    size_t b = to < 0 ? 0 : (size_t)to > t.n ? t.n : (size_t)to;
    return encoded(t.at + a, b > a ? b - a : 0);
}

static mt_atom *chars(const char *text)
{
    codes t = of(text);
    mt_atom *kids[MOST];
    for (size_t i = 0; i < t.n; i++) kids[i] = encoded(&t.at[i], 1);
    return mt_exprv(t.n, kids);
}

static mt_atom *from_codes(const int32_t *at, size_t n)
{
    for (size_t i = 0; i < n; i++) require("Unicode scalar values", utf8proc_codepoint_valid(at[i]));
    return encoded(at, n);
}

/* Joining text values, each coerced, with a separator between them or, for
   lines, a terminator after each. */
static mt_atom *joined(const char *between, const mt_atom *parts, const char *after)
{
    char out[4 * MOST] = "";
    for (size_t i = 0, used = 0; i < mt_len(parts); i++)
        used += (size_t)snprintf(out + used, sizeof out - used, "%s%s%s", i ? between : "", text_of(mt_at(parts, i)), after);
    require("room for the text", strlen(out) < sizeof out - 1);
    return T(out);
}

static bool integral(const mt_atom *n) { return mt_kind_of(n) == MT_INT || mt_kind_of(n) == MT_BIGINT; }

/* Text repeated `times` times, or NULL when times is no integer, which the
   library checks before it looks at the text. Empty text is empty however
   many times it repeats, so C reads no count for it, however wide. TAKES
   times. */
static mt_atom *repeated(const char *text, mt_atom *times)
{
    bool counted = integral(times);
    int64_t n = counted && *text ? mt_int(times) : 0;
    mt_drop(times);
    if (!counted) return NULL;
    codes t = of(text), out = { .n = 0 };
    for (int64_t i = 0; i < n; i++) push(&out, t.at, t.n);
    return encoded(out.at, out.n);
}

/* The library's one padding rule [source: lib/_support/string.metta,
   string-pad; commit=33c2d50c84b24c1a2c906225600e2a4ffdb662f1]: the missing
   codepoints are split by `left`, the rest go right, and each side is the
   filler from its start, repeated and cut to length. An empty filler pads
   nothing, so C reads no width for it. TAKES width. */
static size_t all(size_t missing) { return missing; }
static size_t none(size_t missing) { return missing - missing; }
static size_t half(size_t missing) { return missing / 2; }

static void fill(codes *out, const codes *pad, size_t count)
{
    for (size_t i = 0; pad->n && i < count; i++) push(out, &pad->at[i % pad->n], 1);
}

static mt_atom *padded(const char *text, mt_atom *width, const char *filler, size_t (*left)(size_t))
{
    codes t = of(text), pad = of(filler), out = { .n = 0 };
    require("an integer width", integral(width));
    int64_t w = pad.n ? mt_int(width) : 0;
    mt_drop(width);
    size_t missing = w > (int64_t)t.n ? (size_t)w - t.n : 0;
    fill(&out, &pad, left(missing));
    push(&out, t.at, t.n);
    fill(&out, &pad, missing - left(missing));
    return encoded(out.at, out.n);
}

/* Lines. A blank line holds only spaces and tabs; dedenting empties it and
   indenting leaves it be. */
static size_t leading(const codes *t, span line)
{
    size_t k = line.from;
    while (k < line.to && (t->at[k] == ' ' || t->at[k] == '\t')) k++;
    return k - line.from;
}

static bool blank(const codes *t, span line) { return leading(t, line) == line.to - line.from; }

static mt_atom *lines(const char *text)
{
    codes t = of(text);
    span line[MOST];
    return pieces(&t, line, lines_of(&t, line));
}

/* The common leading spaces and tabs of the nonblank lines come off each;
   SWI's dedent_lines/3 with tabs unexpanded, which string-dedent calls
   [source: lib/lib_string/vendor/string_lines.pl; commit=33c2d50c84b24c1a2c906225600e2a4ffdb662f1]. */
static mt_atom *dedented(const char *text)
{
    codes t = of(text), lf = of("\n"), out = { .n = 0 };
    span line[MOST];
    size_t n = split(&t, exactly, &lf, line), margin = 0, first = n;
    for (size_t i = 0; i < n; i++) {
        if (blank(&t, line[i])) continue;
        size_t lead = leading(&t, line[i]), k = 0;
        if (first == n) first = i, margin = lead;
        while (k < margin && k < lead && t.at[line[i].from + k] == t.at[line[first].from + k]) k++;
        margin = k;
    }
    for (size_t i = 0; i < n; i++) {
        if (i) push(&out, lf.at, 1);
        if (!blank(&t, line[i])) push(&out, t.at + line[i].from + margin, line[i].to - line[i].from - margin);
    }
    return encoded(out.at, out.n);
}

static mt_atom *indented(const char *prefix, const char *text)
{
    codes p = of(prefix), t = of(text), lf = of("\n"), out = { .n = 0 };
    span line[MOST];
    size_t n = split(&t, exactly, &lf, line);
    for (size_t i = 0; i < n; i++) {
        if (i) push(&out, lf.at, 1);
        if (!blank(&t, line[i])) push(&out, p.at, p.n);
        push(&out, t.at + line[i].from, line[i].to - line[i].from);
    }
    return encoded(out.at, out.n);
}

/* SWI's paragraph filler, which string-wrap calls [source: swipl-devel
   library/lynx/format.pl, format_lines/3, take_words/6, justify/2 and
   spread_spc/3; commit=V10.1.14]: a line takes words while they fit its
   width, and a word longer than the width alone. A justified line spreads
   the width it lacks over its gaps, each taking round(spread / gaps left)
   more spaces, half away from zero; the last line stays left. */
typedef enum { LEFT, JUSTIFY } alignment;

static mt_atom *wrapped(const char *text, size_t width, alignment align)
{
    static const char space[] = " \t\n\r";
    codes word[MOST], out = { .n = 0 };
    const int32_t blank_space = ' ', lf = '\n';
    size_t n = 0;
    for (const char *s = text + strspn(text, space); *s; s += strspn(s, space)) {
        char one[4 * MOST];
        size_t len = strcspn(s, space);
        require("room for the words", n < MOST && len < sizeof one);
        memcpy(one, s, len);
        one[len] = '\0';
        word[n++] = of(one);
        s += len;
    }
    for (size_t first = 0, last; first < n; first = last) {
        size_t used = word[first].n;
        for (last = first + 1; last < n && used + 1 + word[last].n <= width; last++) used += 1 + word[last].n;
        size_t spread = align == JUSTIFY && last < n && used < width ? width - used : 0;
        if (first) push(&out, &lf, 1);
        for (size_t w = first; w < last; w++) {
            if (w > first) {
                size_t extra = (size_t)lround((double)spread / (double)(last - w));
                spread -= extra;
                for (size_t k = 0; k <= extra; k++) push(&out, &blank_space, 1);
            }
            push(&out, word[w].at, word[w].n);
        }
    }
    return encoded(out.at, out.n);
}

/* SWI's interpolate_string/4 with goals off, which string-template calls
   [source: swipl-devel library/strings.pl, interpolate//4 and
   exec_interpolate1/3; commit=V10.1.14]: {Name} or {Name,Default}, a name
   being a Prolog variable's, takes its value's console text, a String's own
   text and anything else as the engine shows it; a brace starting neither
   stays text. */
typedef struct binding {
    const char *name;
    const mt_atom *value;
} binding;

static size_t name_length(const char *s)
{
    size_t n = 0;
    if (isupper((unsigned char)*s) || *s == '_')
        while (isalnum((unsigned char)s[n]) || s[n] == '_') n++;
    return n;
}

static mt_atom *interpolated(const char *text, const binding *bindings, size_t count)
{
    char out[4 * MOST] = "";
    size_t used = 0;
    for (const char *s = text; *s;) {
        size_t name = *s == '{' ? name_length(s + 1) : 0;
        const char *close = name ? strchr(s + 1 + name, '}') : NULL;
        if (!close || (s[1 + name] != '}' && s[1 + name] != ',')) {
            used += (size_t)snprintf(out + used, sizeof out - used, "%c", *s++);
            continue;
        }
        const char *shown = NULL;
        for (size_t i = 0; i < count && !shown; i++)
            if (strlen(bindings[i].name) == name && strncmp(bindings[i].name, s + 1, name) == 0)
                shown = mt_kind_of(bindings[i].value) == MT_TEXT ? mt_name(bindings[i].value) : mt_show(bindings[i].value);
        require("a bound name or a default", shown || s[1 + name] == ',');
        if (shown) used += (size_t)snprintf(out + used, sizeof out - used, "%s", shown);
        else used += (size_t)snprintf(out + used, sizeof out - used, "%.*s", (int)(close - (s + 2 + name)), s + 2 + name);
        s = close + 1;
    }
    require("room for the text", used < sizeof out);
    return T(out);
}

/* Levenshtein distance, Wagner-Fischer one row at a time.
   Time: a.n * b.n cell updates. Space: b.n + 1 counts. */
static size_t edit_distance(const char *first, const char *second)
{
    codes a = of(first), b = of(second);
    size_t row[MOST + 1];
    for (size_t j = 0; j <= b.n; j++) row[j] = j;
    for (size_t i = 1; i <= a.n; i++) {
        size_t diagonal = row[0];
        row[0] = i;
        for (size_t j = 1; j <= b.n; j++) {
            size_t above = row[j], best = diagonal + (a.at[i - 1] != b.at[j - 1]);
            if (above + 1 < best) best = above + 1;
            if (row[j - 1] + 1 < best) best = row[j - 1] + 1;
            row[j] = best;
            diagonal = above;
        }
    }
    return row[b.n];
}

static double similarity(const char *first, const char *second)
{
    size_t a = of(first).n, b = of(second).n, longer = a > b ? a : b;
    return longer ? 1.0 - (double)edit_distance(first, second) / (double)longer : 1.0;
}

/* SWI's ISub, the C the library's native object runs [source:
   lib/lib_string/vendor/isub.hpp, adapted from packages-nlp
   dd69ae95342d7a0429a0f8bcc7deab2bd514570e/isub.c;
   commit=33c2d50c84b24c1a2c906225600e2a4ffdb662f1]: cut the longest common
   substring from both while it is longer than the threshold, then weigh what
   they share against what neither matched, plus a bonus for a shared prefix.
   Normalizing lowercases and drops dots, underscores and spaces.
   Time: O(a.n * b.n * a.n) comparisons per cut. */
static codes isub_text(const char *text, bool normalize)
{
    codes t = of(text), out = { .n = 0 };
    for (size_t i = 0; i < t.n; i++) {
        int32_t c = normalize ? utf8proc_tolower(t.at[i]) : t.at[i];
        if (!normalize || (c != '.' && c != '_' && c != ' ')) push(&out, &c, 1);
    }
    return out;
}

static void cut(codes *t, size_t from, size_t to)
{
    memmove(t->at + from, t->at + to, (t->n - to) * sizeof *t->at);
    t->n -= to - from;
}

static double isub(const char *first, const char *second, bool normalize, bool zero_to_one, size_t threshold)
{
    codes a = isub_text(first, normalize), b = isub_text(second, normalize);
    const size_t n1 = a.n, n2 = b.n;
    if (!n1 || !n2) return n1 == n2 ? 1.0 : 0.0;
    size_t prefix = 0, longest = n1 > n2 ? n1 : n2;
    if (threshold > longest) threshold = longest;
    while (prefix < n1 && prefix < n2 && a.at[prefix] == b.at[prefix]) prefix++;
    double common = 0;
    for (size_t best = 2; a.n && b.n && best;) {
        size_t start1 = 0, end1 = 0, start2 = 0, end2 = 0;
        best = 0;
        for (size_t i = 0; i < a.n && a.n - i > best; i++)
            for (size_t j = 0; b.n - j > best;) {
                size_t k = i;
                while (j < b.n && a.at[k] != b.at[j]) j++;
                if (j == b.n) break;
                size_t start = j;
                for (j++, k++; j < b.n && k < a.n && a.at[k] == b.at[j]; j++, k++) {}
                if (k - i > best) best = k - i, start1 = i, end1 = k, start2 = start, end2 = j;
            }
        cut(&a, start1, end1);
        cut(&b, start2, end2);
        if (best > threshold) common += (double)best;
        else best = 0;
    }
    double commonality = 2.0 * common / (double)(n1 + n2);
    double unmatched1 = ((double)n1 - common) / (double)n1, unmatched2 = ((double)n2 - common) / (double)n2;
    double sum = unmatched1 + unmatched2, product = unmatched1 * unmatched2;
    double dissimilarity = sum == product ? 0.0 : product / (0.6 + 0.4 * (sum - product));
    double bonus = (double)(prefix < 4 ? prefix : 4) * 0.1 * (1.0 - commonality);
    double score = commonality - dissimilarity + bonus;
    return zero_to_one ? (score + 1.0) / 2.0 : score;
}

/* A number in Prolog's syntax as far as the original reads it: a 0x, 0o or
   0b radix prefix, else decimal digits, else a float, all of the text or
   none of it. NULL for no number. */
static mt_atom *number_in(const char *text)
{
    static const struct {
        const char *prefix;
        int base;
    } radix[] = { { "0x", 16 }, { "0o", 8 }, { "0b", 2 }, { "", 10 } };
    char *end;
    for (size_t i = 0; i < sizeof radix / sizeof *radix; i++) {
        const char *digits = text + strlen(radix[i].prefix);
        if (strncmp(text, radix[i].prefix, strlen(radix[i].prefix)) != 0 || !isalnum((unsigned char)*digits)) continue;
        long long value = strtoll(digits, &end, radix[i].base);
        if (*end == '\0') return mt_num(value);
    }
    double value = strtod(text, &end);
    return end != text && *end == '\0' ? mt_real(value) : NULL;
}

static void check_number(const char *claim, mt_answers *got, mt_atom *number)
{
    if (number) check_answers(claim, got, number);
    else check_none(claim, got);
}

int main(void)
{
    metta *m = open_engine();
    require("import lib_string", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_string")))));

    const char *fox = "a🦊é";
    check_answers("codepoints, not bytes", mt_eval(m, E("string-length", T(fox))), (int64_t)of(fox).n);
    check_answers("a clamped slice", mt_eval(m, E("string-slice", T(fox), 1, 99)), sliced(fox, 1, 99));
    check_answers("split on any separator", mt_eval(m, E("string-split", T(",;"), T("a,b;;c"))), split_by(",;", "a,b;;c", any_of));
    check_answers("split on the whole separator", mt_eval(m, E("string-split-exact", T("::"), T("::a::::"))),
                  split_by("::", "::a::::", exactly));
    mt_atom *two = E(T("a"), T("b"));
    check_answers("join", mt_eval(m, E("string-join", T(" / "), mt_keep(two))), joined(" / ", two, ""));
    mt_drop(two);
    check_answers("trim", mt_eval(m, E("string-trim", T(" \ta\r\n"))), trimmed(" \ta\r\n"));
    mt_atom *hello = S("hello");
    check_answers("a symbol is text", mt_eval(m, E("string-upper", mt_keep(hello))), cased(text_of(hello), utf8proc_toupper));
    mt_drop(hello);
    check_answers("lower", mt_eval(m, E("string-lower", T("HELLO"))), cased("HELLO", utf8proc_tolower));
    check_answers("starts with", mt_eval(m, E("string-starts-with", T(fox), T("a🦊"))), B(starts_with(fox, "a🦊")));
    check_answers("ends with", mt_eval(m, E("string-ends-with", T(fox), T("é"))), B(ends_with(fox, "é")));
    check_answers("contains", mt_eval(m, E("string-contains", T(fox), T("🦊"))), B(index_of(fox, "🦊") >= 0));
    check_answers("the first index", mt_eval(m, E("string-index-of", T("banana"), T("ana"))), index_of("banana", "ana"));
    check_answers("the last index, overlaps included", mt_eval(m, E("string-last-index-of", T("banana"), T("ana"))),
                  last_index_of("banana", "ana"));
    check_answers("counting without overlap", mt_eval(m, E("string-count", T("aaaaa"), T("aa"))), count("aaaaa", "aa", false));
    check_answers("and with it", mt_eval(m, E("string-count", T("aaaaa"), T("aa"), B(true))), count("aaaaa", "aa", true));
    check_answers("replace", mt_eval(m, E("string-replace", T("aaaaa"), T("aa"), T("X"))), replaced("aaaaa", "aa", "X"));
    check_answers("replacing nothing", mt_eval(m, E("string-replace", T("abc"), T(""), T("X"))), replaced("abc", "", "X"));
    check_answers("nothing last occurs at the end", mt_eval(m, E("string-last-index-of", T("a🦊"), T(""))), last_index_of("a🦊", ""));
    check_answers("nothing occurs at every boundary", mt_eval(m, E("string-count", T("a🦊"), T(""))), count("a🦊", "", false));

    check_answers("chars", mt_eval(m, E("string-chars", T("a🦊"))), chars("a🦊"));
    mt_atom *items = E(T("ab"), "c", 42);
    check_answers("chars from texts, symbols and numbers", mt_eval(m, E("string-from-chars", mt_keep(items))), joined("", items, ""));
    mt_drop(items);
    codes a_fox = of("a🦊");
    check_answers("codes", mt_eval(m, E("string-codes", T("a🦊"))), mt_array(a_fox.n, a_fox.at));
    check_answers("from codes", mt_eval(m, E("string-from-codes", mt_array(a_fox.n, a_fox.at))), from_codes(a_fox.at, a_fox.n));
    static const int32_t with_nul[] = { 97, 0, 129418 };
    mt_atom *nul = from_codes(with_nul, 3);
    codes back = decoded(mt_name(nul), mt_name_len(nul));
    check_answers("a NUL survives the round trip", mt_eval(m, E("string-codes", E("string-from-codes", mt_array(3, with_nul)))),
                  mt_array(back.n, back.at));
    mt_drop(nul);
    check_answers("repeat", mt_eval(m, E("string-repeat", T("ab"), 3)), repeated("ab", mt_num(3)));
    check_answers("pad left", mt_eval(m, E("string-pad-left", T("x"), 4, T("ab"))), padded("x", mt_num(4), "ab", all));
    check_answers("pad right", mt_eval(m, E("string-pad-right", T("x"), 4, T("ab"))), padded("x", mt_num(4), "ab", none));
    check_answers("center", mt_eval(m, E("string-center", T("x"), 6, T("ab"))), padded("x", mt_num(6), "ab", half));

    check_answers("lines", mt_eval(m, E("string-lines", T("a\n\nb\n"))), lines("a\n\nb\n"));
    mt_atom *three = E(T("a"), T(""), T("b"));
    check_answers("unlines", mt_eval(m, E("string-unlines", mt_keep(three))), joined("", three, "\n"));
    mt_drop(three);
    check_answers("dedent", mt_eval(m, E("string-dedent", T("  a\n  b\n"))), dedented("  a\n  b\n"));
    check_answers("indent", mt_eval(m, E("string-indent", T("> "), T("a\n \nb\n"))), indented("> ", "a\n \nb\n"));
    check_answers("wrap", mt_eval(m, E("string-wrap", T("one two three"), 7)), wrapped("one two three", 7, LEFT));
    check_answers("wrap justified", mt_eval(m, E("string-wrap", T("a b c d"), 4, "justify")), wrapped("a b c d", 4, JUSTIFY));
    mt_atom *ada = T("Ada"), *age = E("age", 3);
    const char *letter = "Dear {Name}: {Value}. {Missing,none}";
    check_answers("a template",
                  mt_eval(m, E("string-template", T(letter), E("quote", E(E("Name", mt_keep(ada)), E("Value", mt_keep(age)))))),
                  interpolated(letter, (const binding[]){ { "Name", ada }, { "Value", age } }, 2));
    mt_drop(ada);
    mt_drop(age);

    check_answers("edit distance", mt_eval(m, E("string-edit-distance", T("kitten"), T("sitting"))),
                  (int64_t)edit_distance("kitten", "sitting"));
    check_answers("similarity", mt_eval(m, E("string-similarity", T("ab"), T("ac"))), similarity("ab", "ac"));
    check_answers("ISub of a text with itself", mt_eval(m, E("string-isub", T("language"), T("language"))),
                  isub("language", "language", false, false, 2));
    check_answers("ISub normalized", mt_eval(m, E("string-isub", T("A.B"), T("ab"), E(E("normalize", B(true)), E("zero-to-one", B(true)),
                                                                                         E("substring-threshold", 0)))),
                  isub("A.B", "ab", true, true, 0));
    check_number("a radix prefix", mt_eval(m, E("parse-number", T("0x10"))), number_in("0x10"));
    char digits[24];
    snprintf(digits, sizeof digits, "%d", 42);
    check_answers("a number's text", mt_eval(m, E("number-to-string", 42)), T(digits));
    check_number("no number, no answer", mt_eval(m, E("parse-number", T("not a number"))), number_in("not a number"));

    /* Text recipes over coerced values, huge counts and answer streams. */
    mt_atom *forty_two = mt_num(42), *twelve = mt_num(12), *filler = mt_num(3);
    mt_atom *huge = mt_bigint("1000000000000000000000000");
    check_answers("a number repeats as its text", mt_eval(m, E("string-repeat", mt_keep(forty_two), 2)),
                  repeated(text_of(forty_two), mt_num(2)));
    check_answers("nothing repeats to nothing, however often", mt_eval(m, E("string-repeat", T(""), mt_keep(huge))),
                  repeated("", mt_keep(huge)));
    check_answers("numbers pad as text", mt_eval(m, E("string-center", mt_keep(twelve), 7, mt_keep(filler))),
                  padded(text_of(twelve), mt_num(7), text_of(filler), half));
    check_answers("an empty filler pads nothing, however wide", mt_eval(m, E("string-center", T("x"), mt_keep(huge), T(""))),
                  padded("x", mt_keep(huge), "", half));
    check_answers("a longer suffix does not end a text", mt_eval(m, E("string-ends-with", T("ab"), T("abc"))), B(ends_with("ab", "abc")));
    check_answers("a fractional count is refused",
                  mt_eval(m, E("if-error", E("catch", E("string-repeat", T(""), 1.5)), B(true), B(false))),
                  B(repeated("", mt_real(1.5)) == NULL));
    check_answers("one text per count", mt_eval(m, E("string-repeat", T("x"), E("superpose", E(0, 2)))),
                  repeated("x", mt_num(0)), repeated("x", mt_num(2)));
    mt_drop(forty_two);
    mt_drop(twelve);
    mt_drop(filler);
    mt_drop(huge);

    mt_atom *recipe = NULL;
    mt_rows (row, mt_match(m, E("=", E("string-repeat", V("value"), V("n")), V("body")))) {
        mt_drop(recipe);
        recipe = E("|->", E(mt_keep(mt_bound(row, "value")), mt_keep(mt_bound(row, "n"))), mt_keep(mt_bound(row, "body")));
    }
    require("string-repeat is an equation in &self", recipe != NULL);
    mt_atom *function = mt_one(mt_eval(m, recipe));
    require("the recipe evaluates to a function", function != NULL);
    check_answers("which is the recipe", mt_eval(m, E(function, T("ab"), 2)), repeated("ab", mt_num(2)));
    return done(m);
}
