/* Purpose: lib_parsing, held against a list-of-successes parser written in C,
 *   Hutton and Meijer's model, which the library's own support module cites:
 *   a grammar at a position answers every (value, rest) it can, in order, and
 *   answers none where it cannot, so an ambiguous grammar answers twice and a
 *   repetition answers its longest match first. The grammar is the same term
 *   the engine is given; C reads it through a table of forms, name, argument
 *   kinds and a C function each, which is the library's parsing-form
 *   relation, so adding a form is adding a row at run time, as the original
 *   does. A function inside a grammar is an atom C resolves to a C function
 *   through a table of its own: double and letter? are published to the
 *   engine with mt_def as well, and the grammar-valued sum, field and row are
 *   equations C builds and adds. Input is a token array: the text's
 *   codepoints for grammar-parse, and any atoms for a prepared parser.
 * Build: cc 27-parsing_lib.c $(pkg-config --cflags --libs cmetta libutf8proc)
 * text: lib_parsing's subject is text a grammar reads, so the inputs C hands
 *   it, "(1+2)" among them, are arithmetic text, never MeTTa forms.
 * Assumes: libutf8proc, for splitting text into codepoints and for letter?.
 * Guarantees: all seventy-four claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#if __has_include(<utf8proc.h>)
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utf8proc.h>

/* A door the program needs before it can go on: on refusal, say why and stop. */
#define require(what, ok) \
    ((ok) ? (void)0 : (fprintf(stderr, "%s: %s\n", (what), mt_errmsg()), exit(EXIT_FAILURE)))

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

/* Whether a query answers exactly one value, where a value that is Empty is
   no answer at all: the engine answers a top-level Empty with nothing, and
   inside an expression Empty stays data. Takes both. */
static inline bool value_is(mt_answers *answers, mt_atom *value)
{
    mt_atom *empty = mt_sym("Empty");
    bool nothing = value && mt_eq(value, empty);
    mt_drop(empty);
    if (nothing) mt_drop(value);
    return answers_are(answers, nothing ? mt_exprv(0, NULL) : mt_exprv(1, &value));
}

enum { MOST = 64, FORMS = 32, TOKENS = 64 };

/* Functions a grammar names, resolved by the atom that names them. */
typedef size_t function(const mt_atom *const *args, mt_atom **out);

typedef struct callable {
    mt_atom *atom;
    function *fn;
} callable;

static callable callables[16];
static size_t n_callables;

static mt_atom *registered(mt_atom *atom, function *fn)
{
    require("room for the functions", n_callables < 16);
    callables[n_callables++] = (callable){ mt_keep(atom), fn };
    return atom;
}

static function *resolve(const mt_atom *atom)
{
    for (size_t i = 0; i < n_callables; i++)
        if (mt_eq(callables[i].atom, atom)) return callables[i].fn;
    require("a function C knows", false);
    return NULL;
}

/* The input, and the parses of one grammar at one position: a value, NULL
   for a skipped contribution, and where the rest begins. */
typedef struct input {
    const mt_atom *const *at;
    size_t n;
} input;

typedef struct success {
    mt_atom *value;
    size_t rest;
} success;

typedef struct successes {
    success at[MOST];
    size_t n;
} successes;

static void add(successes *out, mt_atom *value, size_t rest)
{
    require("room for the parses", out->n < MOST);
    out->at[out->n++] = (success){ value, rest };
}

static void release(successes *s)
{
    for (; s->n; s->n--)
        if (s->at[s->n - 1].value) mt_drop(s->at[s->n - 1].value);
}

static mt_atom *surface(const success *s) { return s->value ? mt_keep(s->value) : mt_unit(); }

/* Tokens as text. */
static bool token_is(const mt_atom *token, const char *bytes, size_t len)
{
    return mt_kind_of(token) == MT_TEXT && mt_name_len(token) == len && memcmp(mt_name(token), bytes, len) == 0;
}

static bool in_set(const mt_atom *token, const mt_atom *set)
{
    const char *s = mt_name(set);
    size_t len = mt_name_len(set);
    utf8proc_int32_t c;
    for (utf8proc_ssize_t used; len; s += used, len -= (size_t)used) {
        used = utf8proc_iterate((const utf8proc_uint8_t *)s, (utf8proc_ssize_t)len, &c);
        require("well-formed UTF-8", used > 0);
        if (token_is(token, s, (size_t)used)) return true;
    }
    return false;
}

static mt_atom *text_between(const input *in, size_t from, size_t to)
{
    char buf[4 * TOKENS];
    size_t used = 0;
    for (size_t i = from; i < to; i++) {
        require("room for the text", used + mt_name_len(in->at[i]) < sizeof buf);
        memcpy(buf + used, mt_name(in->at[i]), mt_name_len(in->at[i]));
        used += mt_name_len(in->at[i]);
    }
    return mt_textn(buf, used);
}

static bool one_of(const mt_atom *token, const char *chars)
{
    for (; *chars; chars++)
        if (token_is(token, chars, 1)) return true;
    return false;
}

static bool digit(const mt_atom *t) { return one_of(t, "0123456789"); }
static bool blank(const mt_atom *t) { return one_of(t, "\t\n\v\f\r "); }

/* Where a run of tokens the test accepts ends. */
static size_t span(const input *in, size_t at, bool (*ok)(const mt_atom *), bool want)
{
    while (at < in->n && ok(in->at[at]) == want) at++;
    return at;
}

/* A number's text read as the host reads it: exact without a point or an
   exponent, a float with one. */
static mt_atom *number_of(const input *in, size_t from, size_t to)
{
    mt_atom *text = text_between(in, from, to);
    const char *s = mt_name(text);
    mt_atom *value = strpbrk(s, ".eE") ? mt_real(strtod(s, NULL)) : mt_num(strtoll(s, NULL, 10));
    mt_drop(text);
    return value;
}

/* The forms. */
typedef enum kind { ATOM, TEXT, PARSER, PARSERS } kind;
typedef void run(const mt_atom *const *args, size_t nargs, const input *in, size_t at, successes *out);

typedef struct form {
    const char *name;
    size_t arity;
    kind kinds[3];
    run *fn;
} form;

static void parse(const mt_atom *grammar, const input *in, size_t at, successes *out);

static void run_lit(const mt_atom *const *a, size_t n, const input *in, size_t at, successes *out)
{
    (void)n;
    const char *s = mt_name(a[0]);
    size_t len = mt_name_len(a[0]), pos = at;
    utf8proc_int32_t c;
    for (utf8proc_ssize_t used; len; s += used, len -= (size_t)used, pos++) {
        used = utf8proc_iterate((const utf8proc_uint8_t *)s, (utf8proc_ssize_t)len, &c);
        if (pos >= in->n || !token_is(in->at[pos], s, (size_t)used)) return;
    }
    add(out, mt_keep(a[0]), pos);
}

static void run_any(const mt_atom *const *a, size_t n, const input *in, size_t at, successes *out)
{
    (void)a, (void)n;
    if (at < in->n) add(out, mt_keep(in->at[at]), at + 1);
}

static void run_char_in(const mt_atom *const *a, size_t n, const input *in, size_t at, successes *out)
{
    (void)n;
    if (at < in->n && in_set(in->at[at], a[0])) add(out, mt_keep(in->at[at]), at + 1);
}

static void run_char_not_in(const mt_atom *const *a, size_t n, const input *in, size_t at, successes *out)
{
    (void)n;
    if (at < in->n && !in_set(in->at[at], a[0])) add(out, mt_keep(in->at[at]), at + 1);
}

/* A token whose test answers True, once per True answer. */
static void run_char_if(const mt_atom *const *a, size_t n, const input *in, size_t at, successes *out)
{
    (void)n;
    if (at >= in->n) return;
    mt_atom *verdicts[MOST];
    size_t v = resolve(a[0])(&in->at[at], verdicts);
    for (size_t i = 0; i < v; i++) {
        if (mt_kind_of(verdicts[i]) == MT_BOOL && mt_truth(verdicts[i])) add(out, mt_keep(in->at[at]), at + 1);
        mt_drop(verdicts[i]);
    }
}

static void spanned(const input *in, size_t at, size_t end, size_t minimum, successes *out)
{
    if (end - at >= minimum) add(out, text_between(in, at, end), end);
}

static void run_digits(const mt_atom *const *a, size_t n, const input *in, size_t at, successes *out)
{
    (void)a, (void)n;
    spanned(in, at, span(in, at, digit, true), 1, out);
}

static void run_blanks(const mt_atom *const *a, size_t n, const input *in, size_t at, successes *out)
{
    (void)a, (void)n;
    spanned(in, at, span(in, at, blank, true), 0, out);
}

static void run_nonblanks(const mt_atom *const *a, size_t n, const input *in, size_t at, successes *out)
{
    (void)a, (void)n;
    spanned(in, at, span(in, at, blank, false), 1, out);
}

static void run_until(const mt_atom *const *a, size_t n, const input *in, size_t at, successes *out)
{
    (void)n;
    size_t end = at;
    while (end < in->n && !in_set(in->at[end], a[0])) end++;
    spanned(in, at, end, 0, out);
}

/* An optional sign from `signs`, then digits: where they end, or 0 when
   there are none. */
static size_t signed_digits(const input *in, size_t at, const char *signs)
{
    size_t from = at < in->n && one_of(in->at[at], signs) ? at + 1 : at, end = span(in, from, digit, true);
    return end > from ? end : 0;
}

static void run_integer(const mt_atom *const *a, size_t n, const input *in, size_t at, successes *out)
{
    (void)a, (void)n;
    size_t end = signed_digits(in, at, "-");
    if (end) add(out, number_of(in, at, end), end);
}

/* dcg/basics' number//1: a fraction needs digits after its point and is left
   unread without them; an exponent commits once its e is read. */
static void run_number(const mt_atom *const *a, size_t n, const input *in, size_t at, successes *out)
{
    (void)a, (void)n;
    size_t end = signed_digits(in, at, "-+");
    if (!end) return;
    if (end + 1 < in->n && token_is(in->at[end], ".", 1) && digit(in->at[end + 1])) end = span(in, end + 1, digit, true);
    if (end < in->n && one_of(in->at[end], "eE") && !(end = signed_digits(in, end + 1, "-+"))) return;
    add(out, number_of(in, at, end), end);
}

static void run_quoted(const mt_atom *const *a, size_t n, const input *in, size_t at, successes *out)
{
    (void)a, (void)n;
    static const char escapes[][2] = { { '"', '"' }, { '\\', '\\' }, { 'n', '\n' }, { 't', '\t' } };
    char buf[4 * TOKENS];
    size_t used = 0, pos = at + 1;
    if (at >= in->n || !token_is(in->at[at], "\"", 1)) return;
    for (; pos < in->n && !token_is(in->at[pos], "\"", 1); pos++) {
        const mt_atom *t = in->at[pos];
        if (token_is(t, "\\", 1)) {
            size_t e = 0;
            if (++pos >= in->n) return;
            while (e < 4 && !token_is(in->at[pos], &escapes[e][0], 1)) e++;
            if (e == 4) return;
            buf[used++] = escapes[e][1];
        } else {
            memcpy(buf + used, mt_name(t), mt_name_len(t));
            used += mt_name_len(t);
        }
    }
    if (pos < in->n) add(out, mt_textn(buf, used), pos + 1);
}

static void run_eos(const mt_atom *const *a, size_t n, const input *in, size_t at, successes *out)
{
    (void)a, (void)n;
    if (at == in->n) add(out, mt_unit(), at);
}

static void run_rest(const mt_atom *const *a, size_t n, const input *in, size_t at, successes *out)
{
    (void)a, (void)n;
    add(out, text_between(in, at, in->n), in->n);
}

/* A sequence: every way the parts parse in turn, the values of the parts
   that contribute one gathered in order. */
static void sequence(const mt_atom *const *parts, size_t n, const input *in, size_t at, mt_atom **values, size_t count,
                     successes *out)
{
    if (!n) {
        mt_atom *kept[MOST];
        for (size_t i = 0; i < count; i++) kept[i] = mt_keep(values[i]);
        add(out, mt_exprv(count, kept), at);
        return;
    }
    successes first = { .n = 0 };
    parse(parts[0], in, at, &first);
    for (size_t i = 0; i < first.n; i++) {
        size_t more = count;
        if (first.at[i].value) values[more++] = first.at[i].value;
        sequence(parts + 1, n - 1, in, first.at[i].rest, values, more, out);
    }
    release(&first);
}

static void run_cat(const mt_atom *const *a, size_t n, const input *in, size_t at, successes *out)
{
    mt_atom *values[MOST];
    sequence(a, n, in, at, values, 0, out);
}

static void run_alt(const mt_atom *const *a, size_t n, const input *in, size_t at, successes *out)
{
    for (size_t i = 0; i < n; i++) parse(a[i], in, at, out);
}

static mt_atom *consed(mt_atom *head, const mt_atom *tail)
{
    mt_atom *kids[MOST];
    kids[0] = head;
    for (size_t i = 0; i < mt_len(tail); i++) kids[i + 1] = mt_keep(mt_at(tail, i));
    return mt_exprv(mt_len(tail) + 1, kids);
}

/* A repetition whose step consumes nothing would never end, and the library
   refuses it by name; C raises this flag, errno's way, and the parse that
   started the repetition reads it. */
static bool stalled;

/* One step, then many more: the longest parse comes first. */
static void many_from(const mt_atom *p, const input *in, size_t at, successes *out, bool at_least_one)
{
    successes step = { .n = 0 };
    parse(p, in, at, &step);
    for (size_t i = 0; i < step.n && !stalled; i++) {
        if (step.at[i].rest <= at) {
            stalled = true;
            break;
        }
        successes more = { .n = 0 };
        many_from(p, in, step.at[i].rest, &more, false);
        for (size_t j = 0; j < more.n; j++) add(out, consed(surface(&step.at[i]), more.at[j].value), more.at[j].rest);
        release(&more);
    }
    release(&step);
    if (!at_least_one) add(out, mt_unit(), at);
}

static void run_many(const mt_atom *const *a, size_t n, const input *in, size_t at, successes *out)
{
    (void)n;
    many_from(a[0], in, at, out, false);
}

static void run_many1(const mt_atom *const *a, size_t n, const input *in, size_t at, successes *out)
{
    (void)n;
    many_from(a[0], in, at, out, true);
}

static void run_optional(const mt_atom *const *a, size_t n, const input *in, size_t at, successes *out)
{
    (void)n;
    successes once = { .n = 0 };
    parse(a[0], in, at, &once);
    for (size_t i = 0; i < once.n; i++) add(out, E(surface(&once.at[i])), once.at[i].rest);
    release(&once);
    add(out, mt_unit(), at);
}

/* open, the part, close: the part's contribution alone. */
static void enclosed(const mt_atom *open, const mt_atom *part, const mt_atom *close, const input *in, size_t at, successes *out)
{
    successes o = { .n = 0 };
    parse(open, in, at, &o);
    for (size_t i = 0; i < o.n; i++) {
        successes p = { .n = 0 };
        parse(part, in, o.at[i].rest, &p);
        for (size_t j = 0; j < p.n; j++) {
            successes c = { .n = 0 };
            parse(close, in, p.at[j].rest, &c);
            for (size_t k = 0; k < c.n; k++) add(out, p.at[j].value ? mt_keep(p.at[j].value) : NULL, c.at[k].rest);
            release(&c);
        }
        release(&p);
    }
    release(&o);
}

static void run_between(const mt_atom *const *a, size_t n, const input *in, size_t at, successes *out)
{
    (void)n;
    enclosed(a[0], a[1], a[2], in, at, out);
}

static void run_token(const mt_atom *const *a, size_t n, const input *in, size_t at, successes *out)
{
    (void)n;
    mt_atom *blanks = E("blanks");
    enclosed(blanks, a[0], blanks, in, at, out);
    mt_drop(blanks);
}

/* The parts a separator holds: the first, then (sep part) as often as it
   parses, longest first; or none at all. */
static void run_sep_by(const mt_atom *const *a, size_t n, const input *in, size_t at, successes *out)
{
    (void)n;
    successes first = { .n = 0 };
    mt_atom *step = E("between", mt_keep(a[1]), mt_keep(a[0]), E("cat"));
    parse(a[0], in, at, &first);
    for (size_t i = 0; i < first.n; i++) {
        successes more = { .n = 0 };
        many_from(step, in, first.at[i].rest, &more, false);
        for (size_t j = 0; j < more.n; j++) add(out, consed(surface(&first.at[i]), more.at[j].value), more.at[j].rest);
        release(&more);
    }
    release(&first);
    mt_drop(step);
    add(out, mt_unit(), at);
}

static void run_skip(const mt_atom *const *a, size_t n, const input *in, size_t at, successes *out)
{
    (void)n;
    successes s = { .n = 0 };
    parse(a[0], in, at, &s);
    for (size_t i = 0; i < s.n; i++) add(out, NULL, s.at[i].rest);
    release(&s);
}

/* The part's value through a function, once per answer it gives. */
static void mapped(function *f, const mt_atom *part, const input *in, size_t at, successes *out)
{
    successes s = { .n = 0 };
    parse(part, in, at, &s);
    for (size_t i = 0; i < s.n; i++) {
        mt_atom *value = surface(&s.at[i]), *answers[MOST];
        const mt_atom *arg = value;
        size_t k = f(&arg, answers);
        for (size_t j = 0; j < k; j++) add(out, answers[j], s.at[i].rest);
        mt_drop(value);
    }
    release(&s);
}

static void run_map(const mt_atom *const *a, size_t n, const input *in, size_t at, successes *out)
{
    (void)n;
    mapped(resolve(a[0]), a[1], in, at, out);
}

static void run_as(const mt_atom *const *a, size_t n, const input *in, size_t at, successes *out)
{
    (void)n;
    successes s = { .n = 0 };
    parse(a[1], in, at, &s);
    for (size_t i = 0; i < s.n; i++) add(out, E(mt_keep(a[0]), surface(&s.at[i])), s.at[i].rest);
    release(&s);
}

/* The grammar a function of no arguments answers, parsed where it stands. */
static void run_ref(const mt_atom *const *a, size_t n, const input *in, size_t at, successes *out)
{
    (void)n;
    mt_atom *grammars[MOST];
    size_t k = resolve(a[0])(NULL, grammars);
    for (size_t i = 0; i < k; i++) {
        parse(grammars[i], in, at, out);
        mt_drop(grammars[i]);
    }
}

/* The library's parsing-form rows, in its own order [source:
   lib/_support/parsing.metta; commit=33c2d50c84b24c1a2c906225600e2a4ffdb662f1]. */
static form forms[FORMS] = {
    { "lit", 1, { TEXT }, run_lit },
    { "any", 0, { ATOM }, run_any },
    { "char-in", 1, { TEXT }, run_char_in },
    { "char-not-in", 1, { TEXT }, run_char_not_in },
    { "char-if", 1, { ATOM }, run_char_if },
    { "digits", 0, { ATOM }, run_digits },
    { "integer", 0, { ATOM }, run_integer },
    { "number", 0, { ATOM }, run_number },
    { "blanks", 0, { ATOM }, run_blanks },
    { "nonblanks", 0, { ATOM }, run_nonblanks },
    { "until", 1, { TEXT }, run_until },
    { "quoted", 0, { ATOM }, run_quoted },
    { "eos", 0, { ATOM }, run_eos },
    { "rest", 0, { ATOM }, run_rest },
    { "cat", 0, { PARSERS }, run_cat },
    { "alt", 0, { PARSERS }, run_alt },
    { "many", 1, { PARSER }, run_many },
    { "many1", 1, { PARSER }, run_many1 },
    { "optional", 1, { PARSER }, run_optional },
    { "sep-by", 2, { PARSER, PARSER }, run_sep_by },
    { "between", 3, { PARSER, PARSER, PARSER }, run_between },
    { "skip", 1, { PARSER }, run_skip },
    { "map", 2, { ATOM, PARSER }, run_map },
    { "as", 2, { ATOM, PARSER }, run_as },
    { "token", 1, { PARSER }, run_token },
    { "ref", 1, { ATOM }, run_ref },
};
static size_t n_forms = 26;

static const form *form_of(const mt_atom *g)
{
    if (mt_kind_of(g) != MT_EXPR || !mt_len(g) || mt_kind_of(mt_at(g, 0)) != MT_SYMBOL) return NULL;
    for (size_t i = 0; i < n_forms; i++)
        if (strcmp(forms[i].name, mt_name(mt_at(g, 0))) == 0) return &forms[i];
    return NULL;
}

/* Well formed under the table: a known form, its arity, a text where a Text
   goes and a grammar where a Parser goes. Nothing runs. */
static bool valid(const mt_atom *g)
{
    const form *f = form_of(g);
    if (!f) return false;
    size_t nargs = mt_len(g) - 1;
    if (f->kinds[0] != PARSERS && nargs != f->arity) return false;
    for (size_t i = 0; i < nargs; i++) {
        kind k = f->kinds[0] == PARSERS ? PARSER : f->kinds[i];
        if ((k == TEXT && mt_kind_of(mt_at(g, i + 1)) != MT_TEXT) || (k == PARSER && !valid(mt_at(g, i + 1)))) return false;
    }
    return true;
}

static void parse(const mt_atom *grammar, const input *in, size_t at, successes *out)
{
    const form *f = form_of(grammar);
    require("a well-formed grammar", f != NULL);
    f->fn(mt_children(grammar) + 1, mt_len(grammar) - 1, in, at, out);
}

/* A text's codepoints as one-character text tokens. */
typedef struct chars {
    mt_atom *at[TOKENS];
    size_t n;
} chars;

static chars chars_of(const char *text)
{
    chars c = { .n = 0 };
    size_t len = strlen(text);
    utf8proc_int32_t cp;
    for (utf8proc_ssize_t used; len; text += used, len -= (size_t)used) {
        used = utf8proc_iterate((const utf8proc_uint8_t *)text, (utf8proc_ssize_t)len, &cp);
        require("well-formed UTF-8 in room", used > 0 && c.n < TOKENS);
        c.at[c.n++] = mt_textn(text, (size_t)used);
    }
    return c;
}

static void chars_free(chars *c)
{
    while (c->n) mt_drop(c->at[--c->n]);
}

/* Whether parsing the text makes a repetition stall. */
static bool stalls(const mt_atom *g, const char *text)
{
    chars c = chars_of(text);
    input in = { (const mt_atom *const *)c.at, c.n };
    successes s = { .n = 0 };
    stalled = false;
    parse(g, &in, 0, &s);
    release(&s);
    chars_free(&c);
    return stalled;
}

/* grammar-parse: the value of every parse of the whole text. */
static size_t parses(const mt_atom *g, const char *text, mt_atom **out)
{
    chars c = chars_of(text);
    input in = { (const mt_atom *const *)c.at, c.n };
    successes s = { .n = 0 };
    size_t n = 0;
    stalled = false;
    parse(g, &in, 0, &s);
    require("no repetition stalls", !stalled);
    for (size_t i = 0; i < s.n; i++)
        if (s.at[i].rest == c.n) out[n++] = surface(&s.at[i]);
    release(&s);
    chars_free(&c);
    return n;
}

static mt_atom *the_parse(const mt_atom *g, const char *text)
{
    mt_atom *out[MOST];
    size_t n = parses(g, text, out);
    require("one parse", n == 1);
    return out[0];
}

/* The one answer of a list C computed; TAKES the list. */
static mt_atom *only_first(mt_atom *answers)
{
    require("one answer", mt_len(answers) == 1);
    mt_atom *one = mt_keep(mt_at(answers, 0));
    mt_drop(answers);
    return one;
}

static mt_atom *every_parse(const mt_atom *g, const char *text)
{
    mt_atom *out[MOST];
    return mt_exprv(parses(g, text, out), out);
}

/* grammar-parse-prefix: (Value Unread) for every parse of a prefix. */
static mt_atom *prefix_parses(const mt_atom *g, const char *text)
{
    chars c = chars_of(text);
    input in = { (const mt_atom *const *)c.at, c.n };
    successes s = { .n = 0 };
    mt_atom *out[MOST];
    stalled = false;
    parse(g, &in, 0, &s);
    require("no repetition stalls", !stalled);
    for (size_t i = 0; i < s.n; i++) out[i] = E(surface(&s.at[i]), text_between(&in, s.at[i].rest, c.n));
    size_t n = s.n;
    release(&s);
    chars_free(&c);
    return mt_exprv(n, out);
}

/* A prepared parser's answers over tokens: (Contribution Remainder), the
   contribution () for a skip and (Value) otherwise. */
static mt_atom *prepared(const mt_atom *g, const mt_atom *tokens)
{
    input in = { mt_children(tokens), mt_len(tokens) };
    successes s = { .n = 0 };
    mt_atom *out[MOST];
    stalled = false;
    parse(g, &in, 0, &s);
    require("no repetition stalls", !stalled);
    for (size_t i = 0; i < s.n; i++) {
        mt_atom *rest[TOKENS];
        for (size_t k = s.at[i].rest; k < in.n; k++) rest[k - s.at[i].rest] = mt_keep(in.at[k]);
        out[i] = E(s.at[i].value ? E(mt_keep(s.at[i].value)) : mt_unit(), mt_exprv(in.n - s.at[i].rest, rest));
    }
    size_t n = s.n;
    release(&s);
    return mt_exprv(n, out);
}

/* The functions the original's grammars name. */
static size_t twice(const mt_atom *const *a, mt_atom **out) { return out[0] = mt_num(2 * mt_int(a[0])), 1; }

static size_t letter(const mt_atom *const *a, mt_atom **out)
{
    utf8proc_int32_t c = -1;
    utf8proc_iterate((const utf8proc_uint8_t *)mt_name(a[0]), (utf8proc_ssize_t)mt_name_len(a[0]), &c);
    return out[0] = B(c >= 0 && utf8proc_category_string(c)[0] == 'L'), 1;
}

static size_t arithmetic(const mt_atom *const *a, mt_atom **out) { return (void)a, out[0] = E("+", 1, 2), 1; }
static size_t error_data(const mt_atom *const *a, mt_atom **out) { return (void)a, out[0] = E("Error", "data", "code"), 1; }
static size_t empty_symbol(const mt_atom *const *a, mt_atom **out) { return (void)a, out[0] = S("Empty"), 1; }
static size_t shared(const mt_atom *const *a, mt_atom **out) { return (void)a, out[0] = E(V("x"), V("y"), V("x")), 1; }

/* The grammar-valued functions; each answers the grammar C built for it. */
static mt_atom *sum_grammar, *field_grammar, *row_grammar;
static size_t sum(const mt_atom *const *a, mt_atom **out) { return (void)a, out[0] = mt_keep(sum_grammar), 1; }
static size_t field(const mt_atom *const *a, mt_atom **out) { return (void)a, out[0] = mt_keep(field_grammar), 1; }
static size_t row(const mt_atom *const *a, mt_atom **out) { return (void)a, out[0] = mt_keep(row_grammar), 1; }

/* The form the original adds: its value, without consuming input. */
static void run_pure_value(const mt_atom *const *a, size_t n, const input *in, size_t at, successes *out)
{
    (void)n, (void)in;
    add(out, mt_keep(a[0]), at);
}

typedef struct published {
    function *fn;
} published;

static mt_status applied(mt_call *call, void *user)
{
    const published *p = user;
    const mt_atom *arg = mt_arg(call, 0);
    mt_atom *out[1];
    mt_clear();
    p->fn(&arg, out);
    return mt_ok() ? mt_answer(call, out[0]) : (mt_drop(out[0]), mt_fail(call, "wants its argument"));
}

static mt_atom *verdict(bool holds) { return S(holds ? "fine" : "refused"); }
static mt_atom *guarded(mt_atom *goal) { return E("if-error", E("catch", goal), "refused", "fine"); }
static mt_atom *parsed(mt_atom *grammar, const char *text) { return E("grammar-parse", grammar, T(text)); }

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_parsing", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_parsing")))));
    require("import lib_unicode", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_unicode")))));
    static published letter_p = { letter }, twice_p = { twice };
    require("letter?", mt_def(m, (mt_op){ .name = "letter?", .arity = 1, .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = applied, .user = &letter_p }));
    require("double", mt_def(m, (mt_op){ .name = "double", .arity = 1, .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = applied, .user = &twice_p }));
    mt_drop(registered(S("letter?"), letter));
    mt_drop(registered(S("double"), twice));

    /* A grammar over the whole text: every match an answer, none an answer
       of nothing. */
    mt_atom *out[MOST], *ab = E("lit", T("ab"));
    assert(answers_are(mt_eval(m, parsed(mt_keep(ab), "ab")), E(the_parse(ab, "ab"))) && "a literal");
    assert(answers_are(mt_eval(m, parsed(mt_keep(ab), "ax")), mt_exprv(parses(ab, "ax", out), out)) && "no match, no answer");
    assert(answers_are(mt_eval(m, parsed(mt_keep(ab), "abc")), mt_exprv(parses(ab, "abc", out), out)) && "a prefix is no whole match");

    /* The primitives. */
    static const struct {
        const char *claim, *form, *arg, *text;
    } leaves[] = {
        { "any", "any", NULL, "x" },
        { "char-in", "char-in", "aeiou", "e" },
        { "char-in refuses", "char-in", "aeiou", "z" },
        { "char-not-in", "char-not-in", "aeiou", "z" },
        { "digits", "digits", NULL, "1024" },
        { "integer", "integer", NULL, "-42" },
        { "number", "number", NULL, "3.5" },
        { "nonblanks", "nonblanks", NULL, "word" },
        { "blanks", "blanks", NULL, "  " },
        { "no blanks", "blanks", NULL, "" },
        { "until", "until", ",", "ab" },
        { "quoted", "quoted", NULL, "\"a\\nb\"" },
        { "rest", "rest", NULL, "whatever" },
        { "eos", "eos", NULL, "" },
    };
    for (size_t i = 0; i < sizeof leaves / sizeof *leaves; i++) {
        mt_atom *g = leaves[i].arg ? E(leaves[i].form, T(leaves[i].arg)) : E(leaves[i].form);
        assert(answers_are(mt_eval(m, parsed(mt_keep(g), leaves[i].text)), mt_exprv(parses(g, leaves[i].text, out), out)) && leaves[i].claim);
        mt_drop(g);
    }

    /* char-if applies a function where the grammar was written. */
    mt_atom *letters = E("many1", E("char-if", "letter?")), *one_letter = E("char-if", "letter?");
    assert(answers_are(mt_eval(m, parsed(mt_keep(letters), "héllo")), E(the_parse(letters, "héllo"))) && "a Unicode class");
    assert(answers_are(mt_eval(m, parsed(mt_keep(one_letter), "1")), mt_exprv(parses(one_letter, "1", out), out)) && "a digit is no letter");

    /* cat, skip, alt. */
    mt_atom *grammars[] = {
        E("cat", E("digits"), E("lit", T("-")), E("digits")),
        E("cat", E("digits"), E("skip", E("lit", T("-"))), E("digits")),
        E("cat", E("skip", E("lit", T("#"))), E("rest")),
    };
    const char *texts[] = { "12-34", "12-34", "#tag" }, *claims[] = { "cat", "skip drops a value", "a skipped prefix" };
    for (size_t i = 0; i < 3; i++) {
        assert(answers_are(mt_eval(m, parsed(mt_keep(grammars[i]), texts[i])), E(the_parse(grammars[i], texts[i]))) && claims[i]);
        mt_drop(grammars[i]);
    }
    mt_atom *ambiguous = E("alt", E("digits"), E("nonblanks")), *a_or_b = E("alt", E("lit", T("a")), E("lit", T("b"))),
            *any_or_rest = E("alt", E("any"), E("rest"));
    assert(answers_are(mt_eval(m, E("collapse", parsed(mt_keep(ambiguous), "12"))), E(every_parse(ambiguous, "12"))) && "an ambiguous grammar answers twice");
    assert(answers_are(mt_eval(m, E("collapse", parsed(mt_keep(a_or_b), "a"))), E(every_parse(a_or_b, "a"))) && "one branch");
    assert(answers_are(mt_eval(m, E("collapse", parsed(mt_keep(any_or_rest), "x"))), E(every_parse(any_or_rest, "x"))) && "both branches");
    assert(answers_are(mt_eval(m, E("collapse", parsed(mt_keep(a_or_b), "c"))), E(every_parse(a_or_b, "c"))) && "neither");

    /* many, many1, optional, sep-by. */
    mt_atom *ab_run = E("many", E("char-in", T("ab"))), *ab_run1 = E("many1", E("char-in", T("ab"))),
            *signed_digits = E("cat", E("optional", E("lit", T("-"))), E("digits")),
            *list = E("sep-by", E("digits"), E("lit", T(",")));
    assert(answers_are(mt_eval(m, parsed(mt_keep(ab_run), "aab")), E(the_parse(ab_run, "aab"))) && "many");
    assert(answers_are(mt_eval(m, parsed(mt_keep(ab_run), "")), E(the_parse(ab_run, ""))) && "many of nothing");
    assert(answers_are(mt_eval(m, E("collapse", parsed(mt_keep(ab_run1), ""))), E(every_parse(ab_run1, ""))) && "many1 of nothing");
    assert(answers_are(mt_eval(m, parsed(mt_keep(signed_digits), "-7")), E(the_parse(signed_digits, "-7"))) && "an optional part present");
    assert(answers_are(mt_eval(m, parsed(mt_keep(signed_digits), "7")), E(the_parse(signed_digits, "7"))) && "and absent");
    assert(answers_are(mt_eval(m, parsed(mt_keep(list), "1,2,3")), E(the_parse(list, "1,2,3"))) && "sep-by");
    assert(answers_are(mt_eval(m, parsed(mt_keep(list), "1")), E(the_parse(list, "1"))) && "sep-by of one");
    assert(answers_are(mt_eval(m, parsed(mt_keep(list), "")), E(the_parse(list, ""))) && "sep-by of none");

    /* between, token. */
    mt_atom *brackets = E("between", E("lit", T("(")), E("digits"), E("lit", T(")"))), *tok = E("token", E("digits")),
            *tokens = E("sep-by", E("token", E("digits")), E("lit", T(",")));
    assert(answers_are(mt_eval(m, parsed(mt_keep(brackets), "(5)")), E(the_parse(brackets, "(5)"))) && "between");
    assert(answers_are(mt_eval(m, parsed(mt_keep(tok), "  5 ")), E(the_parse(tok, "  5 "))) && "token");
    assert(answers_are(mt_eval(m, parsed(mt_keep(tokens), " 1 , 2 ")), E(the_parse(tokens, " 1 , 2 "))) && "tokens in a list");

    /* map and as build the tree. */
    mt_atom *doubled = E("map", "double", E("integer")), *amount = E("as", "amount", E("integer")),
            *tagged = E("many", E("as", "digit", E("char-in", T("12"))));
    assert(answers_are(mt_eval(m, parsed(mt_keep(doubled), "21")), E(the_parse(doubled, "21"))) && "map");
    assert(answers_are(mt_eval(m, parsed(mt_keep(amount), "7")), E(the_parse(amount, "7"))) && "as");
    assert(answers_are(mt_eval(m, parsed(mt_keep(tagged), "12")), E(the_parse(tagged, "12"))) && "as inside many");

    /* ref names a grammar a function answers, so a language can nest. */
    sum_grammar = E("alt", E("integer"), E("between", E("lit", T("(")), E("sep-by", E("ref", "sum"), E("lit", T("+"))), E("lit", T(")"))));
    require("sum", mt_add(m, E("=", E("sum"), mt_keep(sum_grammar))));
    mt_drop(registered(S("sum"), sum));
    mt_atom *sums = E("ref", "sum");
    assert(answers_are(mt_eval(m, parsed(mt_keep(sums), "7")), E(the_parse(sums, "7"))) && "a number");
    assert(answers_are(mt_eval(m, parsed(mt_keep(sums), "(1+2)")), E(the_parse(sums, "(1+2)"))) && "a sum");
    assert(answers_are(mt_eval(m, parsed(mt_keep(sums), "(1+(2+3))")), E(the_parse(sums, "(1+(2+3))"))) && "a nested sum");
    assert(answers_are(mt_eval(m, E("collapse", parsed(mt_keep(sums), "(1+)"))), E(every_parse(sums, "(1+)"))) && "an unfinished sum");

    /* A prefix parse answers the value with the unread rest. */
    mt_atom *digits = E("digits"), *rest = E("rest"), *z = E("lit", T("z"));
    assert(answers_are(mt_eval(m, E("grammar-parse-prefix", mt_keep(digits), T("12ab"))), E(only_first(prefix_parses(digits, "12ab")))) && "a prefix");
    mt_atom *longest = prefix_parses(ab_run, "aab!");
    assert(answers_are(mt_eval(m, E("car-atom", E("collapse", E("grammar-parse-prefix", mt_keep(ab_run), T("aab!"))))), E(mt_keep(mt_at(longest, 0))))
           && "the longest comes first");
    mt_drop(longest);
    assert(answers_are(mt_eval(m, E("grammar-parse-prefix", mt_keep(rest), T("all"))), E(only_first(prefix_parses(rest, "all")))) && "everything");
    assert(answers_are(mt_eval(m, E("collapse", E("grammar-parse-prefix", mt_keep(z), T("ab")))), E(prefix_parses(z, "ab"))) && "no prefix");

    /* A CSV row, unambiguous because a bare field stops at a quote. */
    field_grammar = E("alt", E("quoted"), E("until", T(",\"")));
    row_grammar = E("cat", E("sep-by", E("ref", "field"), E("lit", T(","))), E("skip", E("eos")));
    require("field", mt_add(m, E("=", E("field"), mt_keep(field_grammar))));
    require("row", mt_add(m, E("=", E("row"), mt_keep(row_grammar))));
    mt_drop(registered(S("field"), field));
    mt_drop(registered(S("row"), row));
    mt_atom *rows = E("ref", "row");
    assert(answers_are(mt_eval(m, parsed(mt_keep(rows), "a,b,c")), E(the_parse(rows, "a,b,c"))) && "a row of bare fields");
    assert(answers_are(mt_eval(m, parsed(mt_keep(rows), "\"x,y\",z")), E(the_parse(rows, "\"x,y\",z"))) && "a quoted field holds a comma");

    /* The vocabulary is data, and a grammar is checked before any text is read. */
    assert(answers_are(mt_eval(m, E("size-atom", E("grammar-forms"))), E((int64_t)n_forms)) && "the stock forms");
    mt_atom *shaped[] = { E("cat", E("digits"), E("eos")), E("lit", 7), E("many"), mt_num(7) };
    const char *shapes[] = { "a grammar", "a literal needs text", "many needs a part", "a number is no grammar" };
    for (size_t i = 0; i < 4; i++) {
        assert(answers_are(mt_eval(m, E("grammar-is", mt_keep(shaped[i]))), E(B(valid(shaped[i])))) && shapes[i]);
        mt_drop(shaped[i]);
    }
    mt_atom *nosuch = E("nosuch"), *half = E("cat", E("digits"), E("nosuch"));
    assert(answers_are(mt_eval(m, guarded(parsed(mt_keep(nosuch), "a"))), E(verdict(valid(nosuch)))) && "an unknown form");
    assert(answers_are(mt_eval(m, guarded(parsed(mt_keep(half), "1"))), E(verdict(valid(half)))) && "one deep inside");
    mt_atom *seven = mt_num(7);
    assert(answers_are(mt_eval(m, guarded(E("grammar-parse", mt_keep(digits), mt_keep(seven)))), E(verdict(mt_kind_of(seven) == MT_TEXT)))
           && "text must be text");

    /* Zero alternatives answer nothing; zero parts contribute (). */
    mt_atom *no_alt = E("alt"), *no_cat = E("cat"), *all_of = E("many", E("any")), *xs = E("many", E("skip", E("lit", T("x"))));
    assert(answers_are(mt_eval(m, E("grammar-is", mt_keep(no_alt))), E(B(valid(no_alt)))) && "an empty alt is a grammar");
    assert(answers_are(mt_eval(m, E("collapse", parsed(mt_keep(no_alt), ""))), E(every_parse(no_alt, ""))) && "that answers nothing");
    assert(answers_are(mt_eval(m, parsed(mt_keep(no_cat), "")), E(the_parse(no_cat, ""))) && "an empty cat");
    assert(answers_are(mt_eval(m, E("collapse", E("grammar-parse-prefix", mt_keep(all_of), T("ab")))), E(prefix_parses(all_of, "ab")))
           && "every prefix, longest first");
    assert(answers_are(mt_eval(m, parsed(mt_keep(xs), "xx")), E(the_parse(xs, "xx"))) && "skipped repetitions");
    static const struct {
        const char *claim;
        function *fn;
    } values[] = {
        { "runnable data stays data", arithmetic },
        { "an Error expression too", error_data },
        { "Empty is no answer at the top", empty_symbol },
    };
    for (size_t i = 0; i < sizeof values / sizeof *values; i++) {
        mt_atom *made[1];
        values[i].fn(NULL, made);
        mt_atom *lambda = registered(E("|->", E(V("text")), E("quote", made[0])), values[i].fn);
        mt_atom *g = E("map", mt_keep(lambda), E("any"));
        assert(value_is(mt_eval(m, parsed(mt_keep(g), "x")), the_parse(g, "x")) && values[i].claim);
        mt_drop(g);
        mt_drop(lambda);
    }
    mt_atom *xyx = E(V("x"), V("y"), V("x")), *sharing = registered(E("|->", E(V("text")), E("quote", mt_keep(xyx))), shared);
    mt_atom *share_grammar = E("map", mt_keep(sharing), E("any")), *shared_value = the_parse(share_grammar, "x");
    assert(answers_are(mt_eval(m, E("let", V("parsed"), parsed(mt_keep(share_grammar), "x"), E("==", V("parsed"), E("quote", mt_keep(xyx))))), E(B(mt_eq(shared_value, xyx))))
           && "variables keep their sharing");

    /* A prepared parser is a written lambda over tokens. */
    mt_atom *any = E("any"), *data_tokens = E(E("+", 1, 2), "Empty");
    assert(answers_are(mt_eval(m, E("let", V("parser"), E("grammar-parser", mt_keep(any)), E("apply-to", V("parser"), E("quote", E(mt_keep(data_tokens)))))), E(only_first(prepared(any, data_tokens))))
           && "a prepared parser takes any tokens");
    mt_atom *x_lit = E("lit", T("x")), *x_parser = mt_one(mt_eval(m, E("grammar-parser", mt_keep(x_lit))));
    require("a prepared parser", x_parser != NULL);
    assert(answers_are(mt_eval(m, E("let", V("parser"), E("grammar-parser", mt_keep(x_lit)), E("get-metatype", V("parser")))), E(S(mt_kind_of(x_parser) == MT_EXPR ? "Expression" : "Grounded")))
           && "which is an expression");
    mt_atom *compile = NULL;
    mt_rows (r, mt_match(m, E("=", E("grammar-parser", V("grammar")), V("body")))) {
        mt_drop(compile);
        compile = E("|->", E(mt_keep(mt_bound(r, "grammar"))), mt_keep(mt_bound(r, "body")));
    }
    require("grammar-parser is an equation", compile != NULL);
    mt_atom *compiled = mt_one(mt_eval(m, compile)), *chars12 = E(T("1"), T("2"), T("!"));
    require("the recipe evaluates", compiled != NULL);
    assert(answers_are(mt_eval(m, E("let*", E(E(V("compile"), mt_keep(compiled)), E(V("parser"), E("apply-to", V("compile"), E("quote", E(mt_keep(digits)))))),
                                    E("apply-to", V("parser"), E(mt_keep(chars12))))), E(only_first(prepared(digits, chars12))))
           && "the recipe, applied");

    /* One row adds a form, in the engine and in C's table alike. */
    require("the form's row", mt_add(m, E("parsing-form", "pure-value", E("Atom"), "parsing-example-value")));
    require("its type", mt_add(m, E(":", "parsing-example-value", E("->", "Atom", "Atom", "Expression"))));
    require("its function", mt_add(m, E("=", E("parsing-example-value", V("value"), V("input")), E("quote", E(E(V("value")), V("input"))))));
    require("room for a form", n_forms < FORMS);
    forms[n_forms++] = (form){ "pure-value", 1, { ATOM }, run_pure_value };
    mt_atom *pure = E("pure-value", E("+", 1, 2)), *failing = E("ref", E("|->", mt_unit(), E("assertEqual", B(false), B(true))));
    assert(answers_are(mt_eval(m, parsed(mt_keep(pure), "")), E(the_parse(pure, ""))) && "the new form parses");
    assert(answers_are(mt_eval(m, E("size-atom", E("grammar-forms"))), E((int64_t)n_forms)) && "and is listed");
    assert(answers_are(mt_eval(m, E("grammar-is", mt_keep(failing))), E(B(valid(failing)))) && "a ref's function is not run to check it");
    mt_atom *stuck = E("many", E("cat"));
    assert(answers_are(mt_eval(m, guarded(parsed(mt_keep(stuck), ""))), E(verdict(!stalls(stuck, "")))) && "a repeated step must consume input");
    mt_drop(stuck);

    mt_atom *held[] = { ab, letters, one_letter, ambiguous, a_or_b, any_or_rest, ab_run, ab_run1, signed_digits, list, brackets, tok,
                        tokens, doubled, amount, tagged, sums, digits, rest, z, rows, nosuch, half, seven, no_alt, no_cat, all_of, xs,
                        xyx, sharing, share_grammar, shared_value, any, data_tokens, x_lit, x_parser, compiled, chars12, pure, failing,
                        sum_grammar, field_grammar, row_grammar };
    for (size_t i = 0; i < sizeof held / sizeof *held; i++) mt_drop(held[i]);
    for (size_t i = 0; i < n_callables; i++) mt_drop(callables[i].atom);
    mt_close(m);
    return 0;
}
#else
#include <stdio.h>

/* Without utf8proc's headers the program only says what it needs. */
int main(void)
{
    fputs("27-parsing_lib.c needs utf8proc: install its development files, then build with\n"
          "cc 27-parsing_lib.c $(pkg-config --cflags --libs cmetta libutf8proc)\n", stderr);
    return 77;
}
#endif
