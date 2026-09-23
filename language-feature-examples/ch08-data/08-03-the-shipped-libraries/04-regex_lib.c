/* Purpose: lib_regex, held against PCRE2 called from C, the library the
 *   engine's binding wraps. Each pattern is compiled once, in UTF mode as the
 *   binding compiles it, from the very text atom the engine is given. Every
 *   scan is pcre2demo.c's global loop, which the binding follows: after an
 *   empty match it retries at the same place anchored and forbidding an
 *   empty match, and only then steps one character, which is where the
 *   original's empty matches and their nonempty alternatives come from. What
 *   the binding adds, C spells too: captures keyed 0, then by group number,
 *   then by name less its type suffix, in standard order; an optional group
 *   that took no part left out; _I read with strtoll, _R as a range in
 *   characters, _T with sscanf; a replacement as a printf format. (?i), \d,
 *   named groups and a lazy *? are PCRE2's own syntax, so the one library
 *   reads both sides.
 * Assumes: libpcre2-8, which lib_regex's native object links
 *   [source: lib/lib_regex/vendor/VENDOR.md;
 *   commit=33c2d50c84b24c1a2c906225600e2a4ffdb662f1].
 * Guarantees: all twenty-six claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"
#include <ctype.h>
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

enum { MOST = 16 };

/* A pattern as the engine is given it, a text atom, and as PCRE2 compiled it
   from that same text, in the UTF mode the binding compiles with
   [source: lib/lib_regex/vendor/pcre4pl.c, init_re_data;
   commit=33c2d50c84b24c1a2c906225600e2a4ffdb662f1]. */
typedef struct pattern { mt_atom *text; pcre2_code *code; } pattern;

static pattern compiled(mt_atom *text)
{
    int error;
    PCRE2_SIZE at;
    pattern p = { text, pcre2_compile((PCRE2_SPTR)mt_name(text), PCRE2_ZERO_TERMINATED,
                                      PCRE2_UTF, &error, &at, NULL) };
    require("PCRE2 compiles the pattern", p.code != NULL);
    return p;
}

static void release(pattern *p)
{
    pcre2_code_free(p->code);
    mt_drop(p->text);
}

static bool continuation(char byte) { return ((unsigned char)byte & 0xC0) == 0x80; }

/* The characters in n bytes of UTF-8: every byte but a continuation byte,
   10xxxxxx, starts one [source: RFC 3629, section 3]. */
static int64_t chars(const char *s, size_t n)
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
static size_t scan(const pattern *p, const char *s, visit_fn *visit, void *acc)
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
static bool first(const pattern *p, const char *s, uint32_t options, visit_fn *visit, void *acc)
{
    pcre2_match_data *md = pcre2_match_data_create_from_pattern(p->code, NULL);
    require("PCRE2 match data", md != NULL);
    int rc = pcre2_match(p->code, (PCRE2_SPTR)s, strlen(s), 0, options, md, NULL);
    require("pcre2_match", rc > 0 || rc == PCRE2_ERROR_NOMATCH);
    if (rc > 0 && visit) visit(acc, p, s, md);
    pcre2_match_data_free(md);
    return rc > 0;
}

static bool matches(const pattern *p, const char *s) { return first(p, s, 0, NULL, NULL); }

static bool whole_match(const pattern *p, const char *s)
{
    return first(p, s, PCRE2_ANCHORED | PCRE2_ENDANCHORED, NULL, NULL);
}

/* The answers a scan collects. */
typedef struct answers { size_t n; mt_atom *item[MOST]; } answers;

static void push(answers *a, mt_atom *x)
{
    require("room for the answer", a->n < MOST);
    a->item[a->n++] = x;
}

static void whole(void *acc, const pattern *p, const char *s, pcre2_match_data *md)
{
    const PCRE2_SIZE *o = pcre2_get_ovector_pointer(md);
    (void)p;
    push(acc, mt_textn(s + o[0], o[1] - o[0]));
}

static void range(void *acc, const pattern *p, const char *s, pcre2_match_data *md)
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
static int by_key(const void *a, const void *b)
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
static mt_atom *capture_value(char type, const char *s, const PCRE2_SIZE *o, uint32_t i)
{
    const char *from = s + o[2 * i];
    size_t n = o[2 * i + 1] - o[2 * i];
    char text[4 * MOST], op[2];
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
static void captures(void *acc, const pattern *p, const char *s, pcre2_match_data *md)
{
    const PCRE2_SIZE *o = pcre2_get_ovector_pointer(md);
    uint32_t groups, names, entry;
    PCRE2_SPTR table;
    const char *named[MOST] = { 0 };
    capture got[MOST];
    mt_atom *pairs[MOST];
    size_t n = 0;

    pcre2_pattern_info(p->code, PCRE2_INFO_CAPTURECOUNT, &groups);
    pcre2_pattern_info(p->code, PCRE2_INFO_NAMECOUNT, &names);
    pcre2_pattern_info(p->code, PCRE2_INFO_NAMEENTRYSIZE, &entry);
    pcre2_pattern_info(p->code, PCRE2_INFO_NAMETABLE, &table);
    require("room for every group", groups < MOST);
    for (uint32_t i = 0; i < names; i++) {
        PCRE2_SPTR row = table + i * entry;
        named[row[0] << 8 | row[1]] = (const char *)row + 2;
    }
    for (uint32_t i = 0; i <= groups; i++) {
        if (o[2 * i] == PCRE2_UNSET) continue;
        const char *name = named[i], *suffix = name ? strrchr(name, '_') : NULL;
        bool typed = suffix && suffix[1] && !suffix[2];
        size_t width = name ? (typed ? (size_t)(suffix - name) : strlen(name)) : 0;
        char key[4 * MOST];
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

static mt_atom *captures_of(const pattern *p, const char *s)
{
    answers first_match = { 0 };
    require("the pattern matches", first(p, s, 0, captures, &first_match));
    return first_match.item[0];
}

/* A split: the text skipped before each match, the match, and after the
   last match the rest. */
typedef struct pieces { answers parts; size_t here; } pieces;

static void piece(void *acc, const pattern *p, const char *s, pcre2_match_data *md)
{
    pieces *at = acc;
    const PCRE2_SIZE *o = pcre2_get_ovector_pointer(md);
    (void)p;
    push(&at->parts, mt_textn(s + at->here, o[0] - at->here));
    push(&at->parts, mt_textn(s + o[0], o[1] - o[0]));
    at->here = o[1];
}

static mt_atom *split(const pattern *p, const char *s)
{
    pieces at = { { 0 }, 0 };
    scan(p, s, piece, &at);
    push(&at.parts, mt_text(s + at.here));
    return mt_exprv(at.parts.n, at.parts.item);
}

/* A substitution: the text skipped before each match, then the match
   rewritten through a printf format that receives the text of one group, the
   C spelling of a replacement template. */
typedef struct splice { const char *format; uint32_t group; size_t here; char out[8 * MOST]; } splice;

static void substitute(void *acc, const pattern *p, const char *s, pcre2_match_data *md)
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

static mt_atom *replaced(const pattern *p, const char *s, const char *format, uint32_t group, bool all)
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
static mt_atom *quoted(const char *s)
{
    char out[4 * MOST];
    size_t n = 0;
    for (; *s; s++) {
        if ((unsigned char)*s < 0x80 && !isalnum((unsigned char)*s)) out[n++] = '\\';
        out[n++] = *s;
    }
    out[n] = '\0';
    return mt_text(out);
}

static void check_all(const char *claim, mt_answers *got, answers want)
{
    check_answers_(claim, got, want.n, want.item);
}

static answers found(const pattern *p, const char *s, visit_fn *visit)
{
    answers each = { 0 };
    scan(p, s, visit, &each);
    return each;
}

int main(void)
{
    metta *m = open_engine();
    require("import lib_regex", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_regex")))));

    pattern needle = compiled(T("(?i)^needle")), starts_x = compiled(T("^x"));
    check_answers("(?i) folds case", mt_eval(m, E("re-match", mt_keep(needle.text), T("Needle in a haystack"))),
                  B(matches(&needle, "Needle in a haystack")));
    check_answers("^ anchors at the start", mt_eval(m, E("re-match", mt_keep(starts_x.text), T("abc"))),
                  B(matches(&starts_x, "abc")));

    pattern digits = compiled(T("\\d+"));
    check_all("re-find answers every match", mt_eval(m, E("re-find", mt_keep(digits.text), T("a1 b22 c333"))),
              found(&digits, "a1 b22 c333", whole));
    pattern date = compiled(T("(?<year_I>\\d\\d\\d\\d)-(?<month_I>\\d\\d)"));
    check_answers("typed named captures", mt_eval(m, E("re-captures", mt_keep(date.text), T("2017-04-20"))),
                  captures_of(&date, "2017-04-20"));
    pattern colon = compiled(T(":\\s*"));
    check_answers("split keeps what it split on", mt_eval(m, E("re-split", mt_keep(colon.text), T("Age: 33"))),
                  split(&colon, "Age: 33"));
    pattern run_of_a = compiled(T("a+"));
    check_answers("replace every run", mt_eval(m, E("re-replace-all", mt_keep(run_of_a.text), T("X"), T("banana"))),
                  replaced(&run_of_a, "banana", "X", 0, true));
    pattern y = compiled(T("(?<y>\\d+)"));
    check_answers("a named group in the template", mt_eval(m, E("re-replace", mt_keep(y.text), T("[$y]"), T("n 42 n"))),
                  replaced(&y, "n 42 n", "[%.*s]", 1, false));

    /* The native spellings, underscores and all. */
    pattern x = compiled(T("x")), comma = compiled(T(",")), group_x = compiled(T("(x)"));
    check_answers("regex_match", mt_eval(m, E("regex_match", mt_keep(starts_x.text), T("xyz"))),
                  B(matches(&starts_x, "xyz")));
    check_all("regex_find", mt_eval(m, E("regex_find", mt_keep(x.text), T("x-x"))), found(&x, "x-x", whole));
    check_answers("regex_captures", mt_eval(m, E("regex_captures", mt_keep(group_x.text), T("x"))),
                  captures_of(&group_x, "x"));
    check_answers("regex_split", mt_eval(m, E("regex_split", mt_keep(comma.text), T("a,b"))), split(&comma, "a,b"));
    check_answers("regex_replace", mt_eval(m, E("regex_replace", mt_keep(x.text), T("y"), T("xx"))),
                  replaced(&x, "xx", "y", 0, false));
    check_answers("regex_replace_all", mt_eval(m, E("regex_replace_all", mt_keep(x.text), T("y"), T("xx"))),
                  replaced(&x, "xx", "y", 0, true));

    /* A compiled pattern is a value: C holds the engine's, as it holds its
       own pcre2_code, and hands it to a matcher where the text went. */
    mt_atom *held = mt_one(mt_eval(m, E("re-compile", mt_keep(digits.text))));
    check("re-compile answers a value C holds by reference", held && mt_kind_of(held) == MT_HANDLE);
    check_all("which re-find takes as it takes text", mt_eval(m, E("re-find", mt_keep(held), T("n7 n8"))),
              found(&digits, "n7 n8", whole));
    mt_drop(held);

    pattern either = compiled(T("a|ab"));
    check_answers("a full match tries every alternative", mt_eval(m, E("re-fullmatch", mt_keep(either.text), T("ab"))),
                  B(whole_match(&either, "ab")));
    check_answers("and covers the whole text", mt_eval(m, E("re-fullmatch", mt_keep(either.text), T("abc"))),
                  B(whole_match(&either, "abc")));
    pattern n_int = compiled(T("(?<n_I>\\d+)"));
    check_all("re-scan answers each match's captures", mt_eval(m, E("re-scan", mt_keep(n_int.text), T("a1 b22"))),
              found(&n_int, "a1 b22", captures));
    pattern any = compiled(T(".")), nothing = compiled(T(""));
    check_all("ranges count characters, not bytes", mt_eval(m, E("re-ranges", mt_keep(any.text), T("é🦊"))),
              found(&any, "é🦊", range));
    check_answers("an empty pattern matches between every character", mt_eval(m, E("re-count", mt_keep(nothing.text), T("é🦊"))),
                  (int64_t)scan(&nothing, "é🦊", NULL, NULL));
    const char *literal = "a.*\\E #";
    pattern ours = compiled(quoted(literal));
    check_answers("an escaped pattern matches its text whole",
                  mt_eval(m, E("re-fullmatch", E("re-escape", T(literal)), T(literal))), B(whole_match(&ours, literal)));
    pattern theirs = compiled(mt_one(mt_eval(m, E("re-escape", T(literal)))));
    check("and the engine's escaping, compiled by C, does too", whole_match(&theirs, literal));

    /* Empty matches keep their nonempty alternatives; unset groups stay out. */
    pattern lazy = compiled(T("a*?"));
    check_all("a lazy star's empty matches and their alternatives", mt_eval(m, E("re-find", mt_keep(lazy.text), T("aa"))),
              found(&lazy, "aa", whole));
    check_answers("an empty split", mt_eval(m, E("re-split", mt_keep(nothing.text), T("a"))), split(&nothing, "a"));
    check_answers("replacing each of them", mt_eval(m, E("re-replace-all", mt_keep(lazy.text), T("X"), T("aa"))),
                  replaced(&lazy, "aa", "X", 0, true));
    pattern optional = compiled(T("((a)?b)"));
    check_answers("an optional group that took no part", mt_eval(m, E("re-captures", mt_keep(optional.text), T("b"))),
                  captures_of(&optional, "b"));
    pattern term = compiled(T("(?<term_T>1-2)"));
    check_answers("a typed capture substitutes its text",
                  mt_eval(m, E("re-replace", mt_keep(term.text), T("$term"), T("before 1-2 after"))),
                  replaced(&term, "before 1-2 after", "%.*s", 1, false));
    pattern span = compiled(T("(?<span_R>é)(?<term_T>1-2)"));
    check_answers("a range and a term", mt_eval(m, E("re-captures", mt_keep(span.text), T("é1-2"))),
                  captures_of(&span, "é1-2"));

    pattern *all[] = { &needle, &starts_x, &digits, &date, &colon, &run_of_a, &y, &x, &comma, &group_x,
                       &either, &n_int, &any, &nothing, &ours, &theirs, &lazy, &optional, &term, &span };
    for (size_t i = 0; i < sizeof all / sizeof *all; i++) release(all[i]);
    return done(m);
}
