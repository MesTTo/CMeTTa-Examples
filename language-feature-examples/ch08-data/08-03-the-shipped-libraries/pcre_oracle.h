/* Purpose: PCRE2 as the oracle for lib_regex, shared by the twins that call
 *   it: 04-regex_lib and 16-the_prolog_rung. Each pattern is compiled once, in
 *   UTF mode as the binding compiles it, from the very text atom the engine is
 *   given. Every scan is pcre2demo.c's global loop, which the binding follows:
 *   after an empty match it retries at the same place anchored and forbidding
 *   an empty match, and only then steps one character. Captures are keyed 0,
 *   then by group number, then by name less its type suffix, in standard
 *   order; an optional group that took no part is left out; _I is read with
 *   strtoll, _R as a range in characters, _T with sscanf; a replacement is a
 *   printf format. Every function is static inline, so a twin that uses some
 *   of them compiles clean.
 * Assumes: libpcre2-8, which lib_regex's native object links
 *   [source: lib/lib_regex/vendor/VENDOR.md;
 *   commit=33c2d50c84b24c1a2c906225600e2a4ffdb662f1], and common.h included
 *   first with MT_SHORTHAND.
 */
#ifndef PCRE_ORACLE_H
#define PCRE_ORACLE_H
#include <ctype.h>
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

enum { PCRE_MOST = 16 };

/* A pattern as the engine is given it, a text atom, and as PCRE2 compiled it
   from that same text, in the UTF mode the binding compiles with
   [source: lib/lib_regex/vendor/pcre4pl.c, init_re_data;
   commit=33c2d50c84b24c1a2c906225600e2a4ffdb662f1]. */
typedef struct pattern { mt_atom *text; pcre2_code *code; } pattern;

static inline pattern compiled(mt_atom *text)
{
    int error;
    PCRE2_SIZE at;
    pattern p = { text, pcre2_compile((PCRE2_SPTR)mt_name(text), PCRE2_ZERO_TERMINATED,
                                      PCRE2_UTF, &error, &at, NULL) };
    require("PCRE2 compiles the pattern", p.code != NULL);
    return p;
}

static inline void release(pattern *p)
{
    pcre2_code_free(p->code);
    mt_drop(p->text);
}

static inline bool continuation(char byte) { return ((unsigned char)byte & 0xC0) == 0x80; }

/* The characters in n bytes of UTF-8: every byte but a continuation byte,
   10xxxxxx, starts one [source: RFC 3629, section 3]. */
static inline int64_t chars(const char *s, size_t n)
{
    int64_t count = 0;
    for (size_t i = 0; i < n; i++) count += !continuation(s[i]);
    return count;
}

/* What a scan does with each match. */
typedef void visit_fn(void *acc, const pattern *p, const char *s, pcre2_match_data *md);

/* Every match of p in s, left to right, by pcre2demo.c's global loop
   [source: https://github.com/PCRE2Project/pcre2/blob/pcre2-10.46/src/pcre2demo.c;
   lib/lib_regex/vendor/pcre4pl.c, fold_matches;
   commit=33c2d50c84b24c1a2c906225600e2a4ffdb662f1]: after an empty match,
   try again at the same offset anchored and forbidding an empty match, and
   when that fails step one character, both bytes of a CRLF where newlines
   are CRLF; a match whose \K leaves no progress steps past where it started.
   The empty match at the end is the last. Answers how many there were.
   Time: one pcre2_match per match, plus one per failed retry. */
static inline size_t scan(const pattern *p, const char *s, visit_fn *visit, void *acc)
{
    size_t length = strlen(s), start = 0, found = 0;
    uint32_t options = 0, newline;
    pcre2_match_data *md = pcre2_match_data_create_from_pattern(p->code, NULL);
    require("PCRE2 match data", md != NULL);
    pcre2_pattern_info(p->code, PCRE2_INFO_NEWLINE, &newline);
    bool crlf = newline == PCRE2_NEWLINE_ANY || newline == PCRE2_NEWLINE_CRLF ||
                newline == PCRE2_NEWLINE_ANYCRLF;
    for (;;) {
        int rc = pcre2_match(p->code, (PCRE2_SPTR)s, length, start, options, md, NULL);
        if (rc == PCRE2_ERROR_NOMATCH) {
            if (!options) break;
            options = 0;
            if (crlf && start + 1 < length && s[start] == '\r' && s[start + 1] == '\n') start += 2;
            else do start++; while (start < length && continuation(s[start]));
            continue;
        }
        require("pcre2_match", rc > 0);
        const PCRE2_SIZE *o = pcre2_get_ovector_pointer(md);
        found++;
        if (visit) visit(acc, p, s, md);
        start = o[1];
        if (o[0] == o[1]) {
            if (start == length) break;
            options = PCRE2_NOTEMPTY_ATSTART | PCRE2_ANCHORED;
        } else {
            PCRE2_SIZE began = pcre2_get_startchar(md);
            options = 0;
            if (start <= began) {
                if (began >= length) break;
                start = began + 1;
                while (start < length && continuation(s[start])) start++;
            }
        }
    }
    pcre2_match_data_free(md);
    return found;
}

/* The first match of p in s under options, handed to visit; false when
   there is none. */
static inline bool first(const pattern *p, const char *s, uint32_t options, visit_fn *visit, void *acc)
{
    pcre2_match_data *md = pcre2_match_data_create_from_pattern(p->code, NULL);
    require("PCRE2 match data", md != NULL);
    int rc = pcre2_match(p->code, (PCRE2_SPTR)s, strlen(s), 0, options, md, NULL);
    require("pcre2_match", rc > 0 || rc == PCRE2_ERROR_NOMATCH);
    if (rc > 0 && visit) visit(acc, p, s, md);
    pcre2_match_data_free(md);
    return rc > 0;
}

static inline bool matches(const pattern *p, const char *s) { return first(p, s, 0, NULL, NULL); }

static inline bool whole_match(const pattern *p, const char *s)
{
    return first(p, s, PCRE2_ANCHORED | PCRE2_ENDANCHORED, NULL, NULL);
}

/* The answers a scan collects. */
typedef struct answers { size_t n; mt_atom *item[PCRE_MOST]; } answers;

static inline void push(answers *a, mt_atom *x)
{
    require("room for the answer", a->n < PCRE_MOST);
    a->item[a->n++] = x;
}

static inline void whole(void *acc, const pattern *p, const char *s, pcre2_match_data *md)
{
    const PCRE2_SIZE *o = pcre2_get_ovector_pointer(md);
    (void)p;
    push(acc, mt_textn(s + o[0], o[1] - o[0]));
}

static inline void range(void *acc, const pattern *p, const char *s, pcre2_match_data *md)
{
    const PCRE2_SIZE *o = pcre2_get_ovector_pointer(md);
    (void)p;
    push(acc, E(chars(s, o[0]), chars(s + o[0], o[1] - o[0])));
}

/* One capture as the binding keys it: its number, or its name without a
   one-letter type suffix, width bytes long. */
typedef struct capture { const char *name; size_t width; uint32_t number; mt_atom *pair; } capture;

/* The standard order of the keys, which is the order dict_pairs/3 lists the
   binding's dict in: numbers before names, numbers by value, names by their
   bytes [source: SWI-Prolog manual, section 4.6.1, standard order of terms]. */
static inline int by_key(const void *a, const void *b)
{
    const capture *x = a, *y = b;
    if (!x->name != !y->name) return x->name ? 1 : -1;
    if (!x->name) return (x->number > y->number) - (x->number < y->number);
    int order = strncmp(x->name, y->name, x->width < y->width ? x->width : y->width);
    return order ? order : (x->width > y->width) - (x->width < y->width);
}

/* The binding's value for group i: its text, or under a type suffix on its
   name, _I the integer strtoll reads, _R its range in characters as
   (- start length), _T the term it spells, which sscanf reads here as the
   one shape this program's pattern captures, an operator between two
   integers [source: lib/lib_regex/vendor/pcre4pl.c, set_capture_name_and_type
   and put_capval; commit=33c2d50c84b24c1a2c906225600e2a4ffdb662f1]. */
static inline mt_atom *capture_value(char type, const char *s, const PCRE2_SIZE *o, uint32_t i)
{
    const char *from = s + o[2 * i];
    size_t n = o[2 * i + 1] - o[2 * i];
    char text[4 * PCRE_MOST], op[2];
    long long left, right;
    snprintf(text, sizeof text, "%.*s", (int)n, from);
    switch (type) {
    case 'I': return mt_num(strtoll(text, NULL, 10));
    case 'R': return E("-", chars(s, o[2 * i]), chars(from, n));
    case 'T':
        require("the capture is an operator between two integers",
                sscanf(text, "%lld%1[^0-9]%lld", &left, op, &right) == 3);
        return E(mt_sym(op), (int64_t)left, (int64_t)right);
    default: return mt_textn(from, n);
    }
}

/* The match's captures as ((key value) ...): the whole match under 0, a
   group under its number, a named group under its name, keyed in standard
   order, and a group that took no part in the match left out. PCRE2's name
   table gives each name its group, two bytes of number before it. */
static inline void captures(void *acc, const pattern *p, const char *s, pcre2_match_data *md)
{
    const PCRE2_SIZE *o = pcre2_get_ovector_pointer(md);
    uint32_t groups, names, entry;
    PCRE2_SPTR table;
    const char *named[PCRE_MOST] = { 0 };
    capture got[PCRE_MOST];
    mt_atom *pairs[PCRE_MOST];
    size_t n = 0;

    pcre2_pattern_info(p->code, PCRE2_INFO_CAPTURECOUNT, &groups);
    pcre2_pattern_info(p->code, PCRE2_INFO_NAMECOUNT, &names);
    pcre2_pattern_info(p->code, PCRE2_INFO_NAMEENTRYSIZE, &entry);
    pcre2_pattern_info(p->code, PCRE2_INFO_NAMETABLE, &table);
    require("room for every group", groups < PCRE_MOST);
    for (uint32_t i = 0; i < names; i++) {
        PCRE2_SPTR row = table + i * entry;
        named[row[0] << 8 | row[1]] = (const char *)row + 2;
    }
    for (uint32_t i = 0; i <= groups; i++) {
        if (o[2 * i] == PCRE2_UNSET) continue;
        const char *name = named[i], *suffix = name ? strrchr(name, '_') : NULL;
        bool typed = suffix && suffix[1] && !suffix[2];
        size_t width = name ? (typed ? (size_t)(suffix - name) : strlen(name)) : 0;
        char key[4 * PCRE_MOST];
        snprintf(key, sizeof key, "%.*s", (int)width, name ? name : "");
        got[n] = (capture){ name, width, i,
                            E(name ? mt_sym(key) : mt_num(i),
                              capture_value(typed ? suffix[1] : 0, s, o, i)) };
        n++;
    }
    qsort(got, n, sizeof *got, by_key);
    for (size_t k = 0; k < n; k++) pairs[k] = got[k].pair;
    push(acc, mt_exprv(n, pairs));
}

static inline mt_atom *captures_of(const pattern *p, const char *s)
{
    answers first_match = { 0 };
    require("the pattern matches", first(p, s, 0, captures, &first_match));
    return first_match.item[0];
}

/* A split: the text skipped before each match, the match, and after the
   last match the rest. */
typedef struct pieces { answers parts; size_t here; } pieces;

static inline void piece(void *acc, const pattern *p, const char *s, pcre2_match_data *md)
{
    pieces *at = acc;
    const PCRE2_SIZE *o = pcre2_get_ovector_pointer(md);
    (void)p;
    push(&at->parts, mt_textn(s + at->here, o[0] - at->here));
    push(&at->parts, mt_textn(s + o[0], o[1] - o[0]));
    at->here = o[1];
}

static inline mt_atom *split(const pattern *p, const char *s)
{
    pieces at = { { 0 }, 0 };
    scan(p, s, piece, &at);
    push(&at.parts, mt_text(s + at.here));
    return mt_exprv(at.parts.n, at.parts.item);
}

/* A substitution: the text skipped before each match, then the match
   rewritten through a printf format that receives the text of one group, the
   C spelling of a replacement template. */
typedef struct splice { const char *format; uint32_t group; size_t here; char out[8 * PCRE_MOST]; } splice;

static inline void substitute(void *acc, const pattern *p, const char *s, pcre2_match_data *md)
{
    splice *sp = acc;
    const PCRE2_SIZE *o = pcre2_get_ovector_pointer(md);
    size_t used = strlen(sp->out), g = sp->group;
    (void)p;
    snprintf(sp->out + used, sizeof sp->out - used, "%.*s", (int)(o[0] - sp->here), s + sp->here);
    used = strlen(sp->out);
    snprintf(sp->out + used, sizeof sp->out - used, sp->format,
             (int)(o[2 * g + 1] - o[2 * g]), s + o[2 * g]);
    sp->here = o[1];
}

static inline mt_atom *replaced(const pattern *p, const char *s, const char *format, uint32_t group, bool all)
{
    splice sp = { format, group, 0, "" };
    if (all) scan(p, s, substitute, &sp);
    else first(p, s, 0, substitute, &sp);
    size_t used = strlen(sp.out);
    snprintf(sp.out + used, sizeof sp.out - used, "%s", s + sp.here);
    return mt_text(sp.out);
}

/* Literal text as a PCRE2 pattern: a backslash before every ASCII
   character that is not a letter or a digit, which PCRE2 reads as that
   character itself [source: pcre2pattern(3), "BACKSLASH"]. */
static inline mt_atom *quoted(const char *s)
{
    char out[4 * PCRE_MOST];
    size_t n = 0;
    for (; *s; s++) {
        if ((unsigned char)*s < 0x80 && !isalnum((unsigned char)*s)) out[n++] = '\\';
        out[n++] = *s;
    }
    out[n] = '\0';
    return mt_text(out);
}

static inline void check_all(const char *claim, mt_answers *got, answers want)
{
    check_answers_(claim, got, want.n, want.item);
}

static inline answers found(const pattern *p, const char *s, visit_fn *visit)
{
    answers each = { 0 };
    scan(p, s, visit, &each);
    return each;
}

#endif
