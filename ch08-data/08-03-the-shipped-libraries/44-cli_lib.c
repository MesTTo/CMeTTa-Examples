/* Purpose: lib_cli, held against an option parser C writes from the
 *   library's own grammar [source: lib/lib_cli/lib_cli.pl and
 *   lib/lib_cli/vendor/lib_cli_optparse.pl, parse_args_/3 and opt_help_/3;
 *   commit=2803a3ecf877ad410ab19a9168e09096eb50ef5a]. getopt_long is not
 *   that grammar: it clusters -vn and reads -vtrue as five flags, where the
 *   library refuses the first and gives a Boolean the attached value.
 *   Declarations are read and refused as the library refuses them; tokens
 *   are scanned in its order, the terminator, a declared flag, a negated
 *   Boolean, --name=value, -nvalue, then data or an unknown name; every
 *   value is converted before a repeat policy selects, and defaults precede
 *   supplied occurrences. Tokens are counted bytes, so an argument holding
 *   NUL is one argument. A number is a decimal spelling SWI's number syntax
 *   and strtod share, read by mt_bigint and strtod; SWI also reads radix,
 *   character-code, digit-group, rational and infinity spellings and leading
 *   layout, which the original never passes. Each held converter has a C
 *   counterpart, and cli-plus is one C function the engine and C's parser
 *   both call. Help is laid out in the library's columns, padded by
 *   characters where printf pads by bytes.
 * Build: cc 44-cli_lib.c $(pkg-config --cflags --libs cmetta gmp libutf8proc)
 * text: the metta type reads one MeTTa form from its token with mt_parsen,
 *   which is what the library's type does.
 * Guarantees: all sixty-four claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define _POSIX_C_SOURCE 200809L
#define MT_SHORTHAND
#if __has_include(<gmp.h>) && __has_include(<utf8proc.h>)
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_fixtures/exact_oracle.h"
#include <ctype.h>
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

/* The metatype get-metatype answers for an atom of this kind: Symbol for a
   symbol and for a space, which the engine names by a symbol; Variable;
   Expression, the empty one included; Grounded for every value. */
static inline const char *metatype(const mt_atom *atom)
{
    switch (mt_kind_of(atom)) {
    case MT_SYMBOL:
    case MT_SPACE: return "Symbol";
    case MT_VARIABLE: return "Variable";
    case MT_EXPR: return "Expression";
    default: return "Grounded";
    }
}

/* An argument or a name: counted bytes, so one holding NUL stays whole. */
typedef struct token {
    const char *s;
    size_t n;
} token;

#define TOK(literal) ((token){ literal, sizeof literal - 1 })

static bool spelled(token t, const char *text) { return t.n == strlen(text) && memcmp(t.s, text, t.n) == 0; }
static bool starts(token t, const char *prefix) { return t.n >= strlen(prefix) && memcmp(t.s, prefix, strlen(prefix)) == 0; }
static token after(token t, size_t k) { return (token){ t.s + k, t.n - k }; }
static bool same(token a, token b) { return a.n == b.n && memcmp(a.s, b.s, a.n) == 0; }

/* The bytes of the character at the front of t, 0 when t is empty or does
   not start with UTF-8. */
static size_t character(token t)
{
    utf8proc_int32_t c;
    utf8proc_ssize_t k = t.n ? utf8proc_iterate((const utf8proc_uint8_t *)t.s, (utf8proc_ssize_t)t.n, &c) : 0;
    return k > 0 ? (size_t)k : 0;
}

/* Characters, which is what the library's columns count. */
static size_t characters(const char *s, size_t n)
{
    size_t count = 0;
    for (size_t i = 0; i < n; i++) count += ((unsigned char)s[i] & 0xC0) != 0x80;
    return count;
}

/* The number a token spells in decimal, the spelling SWI's number syntax and
   strtod share: [+-] digits [. digits] [(e|E) [+-] digits], an integer when
   it has neither point nor exponent [measured on the patched host:
   number_codes reads "1e10" as a float and refuses ".5", "5." and "2.5e"].
   NULL for anything else. */
static mt_atom *decimal(token t)
{
    size_t i = t.n && (t.s[0] == '+' || t.s[0] == '-'), digits = i;
    bool whole = true;
    while (i < t.n && isdigit((unsigned char)t.s[i])) i++;
    if (i == digits) return NULL;
    if (i < t.n && t.s[i] == '.') {
        size_t fraction = ++i;
        while (i < t.n && isdigit((unsigned char)t.s[i])) i++;
        if (i == fraction) return NULL;
        whole = false;
    }
    if (i < t.n && (t.s[i] == 'e' || t.s[i] == 'E')) {
        i += 1 + (i + 1 < t.n && (t.s[i + 1] == '+' || t.s[i + 1] == '-'));
        size_t exponent = i;
        while (i < t.n && isdigit((unsigned char)t.s[i])) i++;
        if (i == exponent) return NULL;
        whole = false;
    }
    if (i != t.n) return NULL;
    size_t plus = t.s[0] == '+';
    char *text = strndup(t.s + plus, t.n - plus);
    require("room for a number", text != NULL);
    double d = whole ? 0 : strtod(text, NULL);
    mt_atom *out = whole ? mt_bigint(text) : isinf(d) ? NULL : mt_real(d);
    free(text);
    return out;
}

static bool numeric(token t)
{
    mt_atom *n = decimal(t);
    mt_drop(n);
    return n != NULL;
}

/* An expression whose head is the symbol `symbol`. */
static bool headed(const mt_atom *x, const char *symbol)
{
    return mt_kind_of(x) == MT_EXPR && mt_len(x) > 0 && mt_kind_of(mt_at(x, 0)) == MT_SYMBOL && strcmp(mt_name(mt_at(x, 0)), symbol) == 0;
}

static bool ground(const mt_atom *x)
{
    if (mt_kind_of(x) == MT_VARIABLE) return false;
    for (size_t i = 0; mt_kind_of(x) == MT_EXPR && i < mt_len(x); i++)
        if (!ground(mt_at(x, i))) return false;
    return true;
}

/* A Prolog atom: a symbol, or True and False, which are the atoms true and
   false. The empty expression is [], which SWI-Prolog 7 does not count. */
static bool prolog_atom(const mt_atom *x) { return mt_kind_of(x) == MT_SYMBOL || mt_kind_of(x) == MT_BOOL; }

/* A name as the library reads one, a Prolog atom's or a text's characters. */
static bool named(const mt_atom *x, token *out)
{
    if (mt_kind_of(x) == MT_BOOL) return *out = mt_truth(x) ? TOK("true") : TOK("false"), true;
    if (mt_kind_of(x) != MT_SYMBOL && mt_kind_of(x) != MT_TEXT) return false;
    return *out = (token){ mt_name(x), mt_name_len(x) }, true;
}

/* A dashed name's characters: none a separator, a control or =, the
   library's code_type(space) and code_type(cntrl) read as Unicode's Z and Cc
   categories. */
static bool clean(token name)
{
    if (name.n == 0) return false;
    for (size_t k; name.n; name = after(name, k)) {
        utf8proc_int32_t c;
        utf8proc_ssize_t got = utf8proc_iterate((const utf8proc_uint8_t *)name.s, (utf8proc_ssize_t)name.n, &c);
        if (got <= 0) return false;
        utf8proc_category_t category = utf8proc_category(c);
        if (c == '=' || category == UTF8PROC_CATEGORY_CC || category == UTF8PROC_CATEGORY_ZS || category == UTF8PROC_CATEGORY_ZL ||
            category == UTF8PROC_CATEGORY_ZP)
            return false;
        k = (size_t)got;
    }
    return true;
}

/* The built-in types, in cli-types' order. */
enum kind { BOOLEAN, INTEGER, FLOAT, ATOM, STRING, METTA, KINDS, CUSTOM = KINDS };
static const char *const kind_names[KINDS] = { "boolean", "integer", "float", "atom", "string", "metta" };

/* A declaration's fields, in the order the library names them. */
enum field { OPT, TYPE, SHORTS, LONGS, DEFAULT, META, HELP, FIELDS };
static const char *const field_names[FIELDS] = { "opt", "type", "shortflags", "longflags", "default", "meta", "help" };

typedef struct decl {
    const mt_atom *field[FIELDS]; /* each field's value, NULL when absent */
    enum kind kind;
} decl;

/* A held converter's C counterpart: its answers for a token, how many, the
   first two kept, as the library keeps two to tell one answer from more. */
typedef size_t convert_fn(token text, const mt_atom *bound, mt_atom *answers[2]);

/* a + b as SWI adds them: exactly for two exact numbers, as doubles when
   either is a float. */
static mt_atom *sum(const mt_atom *a, const mt_atom *b)
{
    if (mt_kind_of(a) == MT_FLOAT || mt_kind_of(b) == MT_FLOAT) return mt_real(nearest(a) + nearest(b));
    mpq_t x, y;
    mpq_inits(x, y, NULL);
    exact(x, a), exact(y, b);
    mpq_add(x, x, y);
    mt_atom *out = rational_of(x);
    mpq_clears(x, y, NULL);
    return out;
}

static size_t parse_number(token text, const mt_atom *bound, mt_atom *answers[2])
{
    (void)bound;
    return (answers[0] = decimal(text)) != NULL;
}

/* (cli-plus increment text): the increment plus the number the text spells,
   or no answer, as (+ $increment (parse-number $text)) has none then. */
static size_t plus(token text, const mt_atom *increment, mt_atom *answers[2])
{
    mt_atom *n = decimal(text);
    if (!n) return 0;
    answers[0] = sum(increment, n);
    mt_drop(n);
    return 1;
}

static mt_status cli_plus(mt_call *call, void *user)
{
    (void)user;
    const mt_atom *increment = mt_arg(call, 0), *text = mt_arg(call, 1);
    mt_atom *answer[2];
    if (!is_number(increment) || mt_kind_of(text) != MT_TEXT) return mt_fail(call, "cli-plus adds a number to a number's text");
    return plus((token){ mt_name(text), mt_name_len(text) }, increment, answer) ? mt_answer(call, answer[0]) : MT_FAIL;
}

/* string-upper: each character's simple uppercase mapping, utf8proc's. */
static size_t upper(token text, const mt_atom *bound, mt_atom *answers[2])
{
    (void)bound;
    char *out = malloc(4 * text.n + 1);
    size_t n = 0;
    require("room for the uppercase", out != NULL);
    for (size_t k; text.n; text = after(text, k)) {
        utf8proc_int32_t c;
        utf8proc_ssize_t got = utf8proc_iterate((const utf8proc_uint8_t *)text.s, (utf8proc_ssize_t)text.n, &c);
        require("UTF-8 text", got > 0);
        n += (size_t)utf8proc_encode_char(utf8proc_toupper(c), (utf8proc_uint8_t *)out + n);
        k = (size_t)got;
    }
    answers[0] = mt_textn(out, n);
    free(out);
    return 1;
}

static size_t nothing(token text, const mt_atom *bound, mt_atom *answers[2]) { return (void)text, (void)bound, (void)answers, 0; }

static size_t one_and_two(token text, const mt_atom *bound, mt_atom *answers[2])
{
    (void)text, (void)bound;
    answers[0] = N(1), answers[1] = N(2);
    return 2;
}

static size_t wrong(token text, const mt_atom *bound, mt_atom *answers[2]) { return (void)text, (void)bound, answers[0] = T("wrong"), 1; }

static mt_atom *lambda(mt_atom *body) { return E("|->", E(V("x")), body); }

/* The C counterpart of a held function, found by its structure: a partial
   application of cli-plus binds its increment, and the others are the
   function the original writes or a lambda alpha-equal to it. NULL for one
   this program has no counterpart for. */
static convert_fn *counterpart(const mt_atom *function, const mt_atom **bound)
{
    *bound = NULL;
    if (headed(function, "cli-plus") && mt_len(function) == 2) return *bound = mt_at(function, 1), plus;
    struct { mt_atom *written; convert_fn *convert; } known[] = {
        { S("parse-number"), parse_number },    { lambda(E("string-upper", V("x"))), upper }, { lambda(E("empty")), nothing },
        { lambda(E("superpose", E(1, 2))), one_and_two }, { lambda(T("wrong")), wrong },
    };
    convert_fn *found = NULL;
    for (size_t i = 0; i < sizeof known / sizeof *known; i++) {
        if (!found && mt_alpha_eq(function, known[i].written)) found = known[i].convert;
        mt_drop(known[i].written);
    }
    return found;
}

/* A declared custom type, checked in C: Number, String, and (Annotated T (Gt
   k)), the refinement the original's positive size declares. */
static bool of_type(const mt_atom *value, const mt_atom *type)
{
    if (mt_kind_of(type) == MT_SYMBOL && strcmp(mt_name(type), "Number") == 0) return is_number(value);
    if (mt_kind_of(type) == MT_SYMBOL && strcmp(mt_name(type), "String") == 0) return mt_kind_of(value) == MT_TEXT;
    if (headed(type, "Annotated") && mt_len(type) == 3 && headed(mt_at(type, 2), "Gt") && mt_len(mt_at(type, 2)) == 2)
        return of_type(value, mt_at(type, 1)) && is_number(value) && mt_compare(value, mt_at(mt_at(type, 2), 1)) > 0;
    require("a type this program checks", false);
    return false;
}

/* Whether a value is of a declaration's type: must_be's built-in types, a
   text for string, any term for metta, a custom type's own check. */
static bool typed(const decl *d, const mt_atom *v)
{
    switch (d->kind) {
    case BOOLEAN: return mt_kind_of(v) == MT_BOOL;
    case INTEGER: return mt_kind_of(v) == MT_INT || mt_kind_of(v) == MT_BIGINT;
    case FLOAT: return mt_kind_of(v) == MT_FLOAT;
    case ATOM: return prolog_atom(v);
    case STRING: return mt_kind_of(v) == MT_TEXT;
    case METTA: return true;
    default: return of_type(v, mt_at(d->field[TYPE], 1));
    }
}

/* One row's fields: two-item, from the seven names, each at most once, with
   opt present. */
static bool fields(const mt_atom *row, decl *d)
{
    if (mt_kind_of(row) != MT_EXPR) return false;
    for (size_t i = 0; i < mt_len(row); i++) {
        const mt_atom *f = mt_at(row, i);
        size_t which = FIELDS;
        for (size_t k = 0; k < FIELDS && mt_kind_of(f) == MT_EXPR && mt_len(f) == 2; k++)
            if (mt_kind_of(mt_at(f, 0)) == MT_SYMBOL && strcmp(mt_name(mt_at(f, 0)), field_names[k]) == 0) which = k;
        if (which == FIELDS || d->field[which]) return false;
        d->field[which] = mt_at(f, 1);
    }
    return d->field[OPT] != NULL;
}

/* The row's type: a built-in name, absent meaning string, or (parse Expected
   Function) with a ground Expected. */
static bool kind(decl *d)
{
    const mt_atom *type = d->field[TYPE];
    if (!type) return d->kind = STRING, true;
    for (size_t k = 0; k < KINDS; k++)
        if (mt_kind_of(type) == MT_SYMBOL && strcmp(mt_name(type), kind_names[k]) == 0) return d->kind = k, true;
    d->kind = CUSTOM;
    return headed(type, "parse") && mt_len(type) == 3 && ground(mt_at(type, 1)) && mt_kind_of(mt_at(type, 2)) != MT_VARIABLE;
}

/* A list of names each clean, a short one a single character other than -. */
static bool names(const mt_atom *list, bool shorts)
{
    if (!list) return true;
    if (mt_kind_of(list) != MT_EXPR) return false;
    for (size_t i = 0; i < mt_len(list); i++) {
        token name;
        if (!named(mt_at(list, i), &name) || !clean(name)) return false;
        if (shorts && (character(name) != name.n || spelled(name, "-"))) return false;
    }
    return true;
}

static bool texts(const mt_atom *list)
{
    for (size_t i = 0; i < mt_len(list); i++)
        if (mt_kind_of(mt_at(list, i)) != MT_TEXT) return false;
    return true;
}

/* Two declarations share a complete dashed name: -x and --x are different
   names, since the two namespaces are separate. */
static bool clash(const decl *a, const decl *b)
{
    for (enum field f = SHORTS; f <= LONGS; f++)
        for (size_t i = 0; a->field[f] && b->field[f] && i < mt_len(a->field[f]); i++)
            for (size_t j = 0; j < mt_len(b->field[f]); j++) {
                token x, y;
                if ((a != b || i != j) && named(mt_at(a->field[f], i), &x) && named(mt_at(b->field[f], j), &y) && same(x, y)) return true;
            }
    return false;
}

/* A specification's declarations, or NULL where the library refuses it:
   rows of fields, opt a Prolog atom unique across rows, a known type,
   clean and unique dashed names, meta a text, help a text or texts, and a
   ground default of the declared type. */
static decl *declared(const mt_atom *spec, size_t *count)
{
    if (mt_kind_of(spec) != MT_EXPR) return NULL;
    size_t n = mt_len(spec);
    decl *d = calloc(n + 1, sizeof *d);
    bool fine = d != NULL;
    require("room for declarations", fine);
    for (size_t i = 0; fine && i < n; i++) {
        decl *r = &d[i];
        fine = fields(mt_at(spec, i), r) && prolog_atom(r->field[OPT]) && kind(r) && names(r->field[SHORTS], true) &&
               names(r->field[LONGS], false) && (!r->field[META] || mt_kind_of(r->field[META]) == MT_TEXT) &&
               (!r->field[HELP] || mt_kind_of(r->field[HELP]) == MT_TEXT || (mt_kind_of(r->field[HELP]) == MT_EXPR && texts(r->field[HELP]))) &&
               (!r->field[DEFAULT] || (ground(r->field[DEFAULT]) && typed(r, r->field[DEFAULT])));
        for (size_t j = 0; fine && j <= i; j++) fine = !(j < i && mt_eq(d[j].field[OPT], r->field[OPT])) && !clash(&d[j], r);
    }
    if (!fine) return free(d), NULL;
    *count = n;
    return d;
}

/* The declaration whose short or long name is `name`. */
static const decl *flagged(const decl *d, size_t n, enum field f, token name)
{
    for (size_t i = 0; i < n; i++)
        for (size_t j = 0; d[i].field[f] && j < mt_len(d[i].field[f]); j++) {
            token x;
            if (named(mt_at(d[i].field[f], j), &x) && same(x, name)) return &d[i];
        }
    return NULL;
}

/* The declaration a whole token names as -x or --name. */
static const decl *flag(const decl *d, size_t n, token t)
{
    if (starts(t, "--")) return flagged(d, n, LONGS, after(t, 2));
    return starts(t, "-") ? flagged(d, n, SHORTS, after(t, 1)) : NULL;
}

/* A token that would read as an option: dash-led, but neither a lone dash
   nor a number. */
static bool option_shaped(token t) { return starts(t, "-") && !spelled(t, "-") && !numeric(t); }

/* Why a parse refused, and the token it refused at. */
typedef struct refusal {
    const char *why;
    token at;
} refusal;

/* An occurrence of a declaration, or an operand when `of` is NULL. */
typedef struct item {
    const decl *of;
    token text;
    mt_atom *value;
} item;

static bool refuse(refusal *r, const char *why, token at) { return r->why = why, r->at = at, false; }

/* A built-in value from its token, converted as the library's scanner does:
   Booleans from true and false, integers and floats from their decimal
   spellings, an atom's name as a symbol with true and false the Booleans. A
   string, metta or custom value waits for the text. */
static bool scanned_value(item *it, refusal *r, token at)
{
    token t = it->text;
    switch (it->of->kind) {
    case BOOLEAN: it->value = spelled(t, "true") ? B(true) : spelled(t, "false") ? B(false) : NULL; break;
    case INTEGER:
    case FLOAT:
        it->value = decimal(t);
        if (it->value && (mt_kind_of(it->value) == MT_FLOAT) != (it->of->kind == FLOAT)) mt_drop(it->value), it->value = NULL;
        break;
    case ATOM: {
        char *name = strndup(t.s, t.n);
        require("an atom name C can spell, with no NUL", name && strlen(name) == t.n);
        it->value = spelled(t, "true") ? B(true) : spelled(t, "false") ? B(false) : S(name);
        free(name);
        break;
    }
    default: return true;
    }
    return it->value || refuse(r, "Supply a value of the declared type", at);
}

/* A string, metta or custom value from its text, after the whole scan: a
   text, one MeTTa form, or the converter's single answer of the declared
   type. */
static bool converted(item *it, refusal *r)
{
    token t = it->text;
    if (it->of->kind == STRING) return it->value = mt_textn(t.s, t.n), true;
    if (it->of->kind == METTA) {
        if ((it->value = mt_parsen(t.s, t.n))) return true;
        mt_clear(); /* the reader's refusal becomes this parse's, handled here */
        return refuse(r, "Supply one MeTTa form", t);
    }
    const mt_atom *bound;
    convert_fn *convert = counterpart(mt_at(it->of->field[TYPE], 2), &bound);
    require("a counterpart for the held converter", convert != NULL);
    mt_atom *answers[2] = { NULL, NULL };
    size_t got = convert(t, bound, answers);
    for (size_t i = 1; i < got && i < 2; i++) mt_drop(answers[i]);
    if (got == 1 && typed(it->of, answers[0])) return it->value = answers[0], true;
    if (got) mt_drop(answers[0]);
    return refuse(r, got == 1 ? "Return a value that passes the specified type" : "Make the decoder return exactly one value", t);
}

/* The tokens of an argument vector: an expression of texts, or false. */
static bool tokens_of(const mt_atom *arguments, token *out)
{
    if (mt_kind_of(arguments) != MT_EXPR || !texts(arguments)) return false;
    for (size_t i = 0; i < mt_len(arguments); i++) out[i] = (token){ mt_name(mt_at(arguments, i)), mt_name_len(mt_at(arguments, i)) };
    return true;
}

/* Scan tokens into items in the library's order: -- makes every later
   token an operand; a declared -x or --name takes the next token (a
   Boolean only true or false, anything else one that does not read as an
   option); --no-name is false for a declared Boolean; --name=value splits at
   the first =; -xvalue attaches, and -x=value is refused; a lone dash, a
   token not dash-led and a number are operands; any other dash-led token
   names nothing declared. */
static bool scan(const decl *d, size_t n, const token *tokens, size_t argc, item *items, size_t *count, refusal *r)
{
    *count = 0;
    for (size_t i = 0; i < argc;) {
        token t = tokens[i++], name, value = { 0 };
        if (spelled(t, "--")) {
            while (i < argc) items[(*count)++] = (item){ NULL, tokens[i++], NULL };
            break;
        }
        const decl *of = flag(d, n, t);
        bool attached = true;
        size_t x;
        if (of) attached = false;
        else if (starts(t, "--no-") && (of = flagged(d, n, LONGS, after(t, 5))) && of->kind == BOOLEAN) value = TOK("false");
        else if (starts(t, "--") && memchr(t.s + 2, '=', t.n - 2)) {
            name = (token){ t.s + 2, (size_t)((const char *)memchr(t.s + 2, '=', t.n - 2) - (t.s + 2)) };
            if (!(of = flagged(d, n, LONGS, name))) return refuse(r, "Correct or declare this name", (token){ t.s, name.n + 2 });
            value = after(t, name.n + 3);
        } else if (starts(t, "-") && !starts(t, "--") && (x = character(after(t, 1))) && t.n > 1 + x &&
                   (of = flagged(d, n, SHORTS, (token){ t.s + 1, x }))) {
            if (t.s[1 + x] == '=') return refuse(r, "Use -n value, -nvalue or --name=value", t);
            value = after(t, 1 + x);
        } else if (spelled(t, "-") || !starts(t, "-") || numeric(t)) {
            items[(*count)++] = (item){ NULL, t, NULL };
            continue;
        } else
            return refuse(r, "Correct or declare this name", t);
        if (!attached && of->kind == BOOLEAN)
            value = i < argc && (spelled(tokens[i], "true") || spelled(tokens[i], "false")) ? tokens[i++] : TOK("true");
        else if (!attached) {
            if (i == argc || option_shaped(tokens[i])) return refuse(r, "Supply its value", t);
            value = tokens[i++];
        }
        items[*count] = (item){ of, value, NULL };
        if (!scanned_value(&items[(*count)++], r, t)) return false;
    }
    return true;
}

/* Whether a repeat policy keeps occurrence i: all of them, the first of each
   declaration, or the last. */
static bool kept(const item *items, size_t count, size_t i, const char *policy)
{
    bool first = strcmp(policy, "keepfirst") == 0;
    if (strcmp(policy, "keepall") == 0) return true;
    for (size_t j = first ? 0 : i + 1; j < (first ? i : count); j++)
        if (items[j].of == items[i].of) return false;
    return true;
}

/* What cli-parse answers: ((Key Value) ...) then (Operand ...), defaults for
   the declarations nothing supplied first, in declaration order, then the
   occurrences the policy keeps, in input order; NULL with `r` filled in
   where the library refuses. */
static mt_atom *parsed(const mt_atom *spec, const mt_atom *arguments, const char *policy, refusal *r)
{
    *r = (refusal){ NULL, { "", 0 } };
    if (strcmp(policy, "keepfirst") && strcmp(policy, "keeplast") && strcmp(policy, "keepall"))
        return refuse(r, "Choose keepfirst, keeplast or keepall", (token){ policy, strlen(policy) }), NULL;
    size_t n, argc = mt_kind_of(arguments) == MT_EXPR ? mt_len(arguments) : 0, count = 0;
    decl *d = declared(spec, &n);
    token *tokens = malloc((argc + 1) * sizeof *tokens);
    item *items = calloc(argc + 1, sizeof *items);
    require("room for the scan", tokens && items);
    bool fine = (d || refuse(r, "Correct this option declaration", (token){ "", 0 })) &&
                (tokens_of(arguments, tokens) || refuse(r, "Pass a list of texts", (token){ "", 0 })) &&
                scan(d, n, tokens, argc, items, &count, r);
    for (size_t i = 0; fine && i < count; i++) fine = !items[i].of || items[i].value || converted(&items[i], r);
    mt_atom *out = NULL;
    if (fine) {
        mt_atom **pairs = malloc((n + count + 1) * sizeof *pairs), **operands = malloc((count + 1) * sizeof *operands);
        size_t p = 0, o = 0;
        require("room for the answer", pairs && operands);
        for (size_t i = 0; i < n; i++) {
            bool supplied = false;
            for (size_t j = 0; j < count; j++) supplied |= items[j].of == &d[i];
            if (!supplied && d[i].field[DEFAULT]) pairs[p++] = E(mt_keep(d[i].field[OPT]), mt_keep(d[i].field[DEFAULT]));
        }
        for (size_t i = 0; i < count; i++)
            if (!items[i].of) operands[o++] = mt_textn(items[i].text.s, items[i].text.n);
            else if (kept(items, count, i, policy)) pairs[p++] = E(mt_keep(items[i].of->field[OPT]), mt_keep(items[i].value));
        out = E(mt_exprv(p, pairs), mt_exprv(o, operands));
        free(pairs), free(operands);
    }
    for (size_t i = 0; i < count; i++) mt_drop(items[i].value);
    free(d), free(tokens), free(items);
    return out;
}

/* A growing text: the help as it is laid out. */
typedef struct text {
    char *s;
    size_t n, cap;
} text;

static void put(text *b, const char *s, size_t n)
{
    if (b->n + n + 1 > b->cap) {
        b->cap = 2 * (b->n + n + 1);
        b->s = realloc(b->s, b->cap);
        require("room for the help", b->s != NULL);
    }
    memcpy(b->s + b->n, s, n);
    b->s[b->n += n] = '\0';
}

static void puts_(text *b, const char *s) { put(b, s, strlen(s)); }

/* Spaces to a column, counted in characters from the start of the line, as
   format/2's ~t~*+ pads. */
static void pad_to(text *b, size_t column)
{
    size_t start = b->n;
    while (start > 0 && b->s[start - 1] != '\n') start--;
    for (size_t at = characters(b->s + start, b->n - start); at < column; at++) put(b, " ", 1);
}

/* Words grouped into lines of at most `width` characters, a word too long
   for any line alone on its own, as optparse's group_length/3 groups them;
   `into` gets the groups joined by `within` and separated by `between`. */
static void grouped(text *into, const token *words, size_t count, size_t width, const char *within, const char *between)
{
    ptrdiff_t remains = (ptrdiff_t)width;
    bool line_empty = true;
    for (size_t i = 0; i < count; i++) {
        ptrdiff_t k = (ptrdiff_t)characters(words[i].s, words[i].n);
        if (!line_empty && remains < k) puts_(into, between), remains = (ptrdiff_t)width, line_empty = true;
        if (!line_empty) puts_(into, within);
        put(into, words[i].s, words[i].n);
        remains -= k + 1, line_empty = false;
    }
}

/* A value as the engine's writer spells it, for a type or a default in the
   help. */
static void put_written(text *b, const mt_atom *x)
{
    char *s = mt_show_dup(x);
    require("a written value", s != NULL);
    puts_(b, s);
    mt_free(s);
}

/* One declaration's three leading columns: its short names, its dashed long
   names and its meta:type=default. */
typedef struct columns {
    text shorts, meta;
    token *longs;
    size_t n_longs;
} columns;

static columns columns_of(const decl *d)
{
    columns c = { 0 };
    size_t n = d->field[LONGS] ? mt_len(d->field[LONGS]) : 0;
    puts_(&c.shorts, ""), puts_(&c.meta, "");
    c.longs = calloc(n + 1, sizeof *c.longs);
    require("room for the long names", c.longs != NULL);
    for (size_t i = 0; d->field[SHORTS] && i < mt_len(d->field[SHORTS]); i++) {
        token name;
        named(mt_at(d->field[SHORTS], i), &name);
        puts_(&c.shorts, i ? ",-" : "-"), put(&c.shorts, name.s, name.n);
    }
    for (size_t i = 0; i < n; i++) {
        token name;
        named(mt_at(d->field[LONGS], i), &name);
        char *dashed = malloc(name.n + 3);
        require("room for a long name", dashed != NULL);
        memcpy(dashed, "--", 2), memcpy(dashed + 2, name.s, name.n), dashed[name.n + 2] = '\0';
        c.longs[c.n_longs++] = (token){ dashed, name.n + 2 };
    }
    if (d->field[META] && mt_name_len(d->field[META])) put(&c.meta, mt_name(d->field[META]), mt_name_len(d->field[META])), puts_(&c.meta, ":");
    if (d->kind == CUSTOM) put_written(&c.meta, mt_at(d->field[TYPE], 1));
    else puts_(&c.meta, kind_names[d->kind]);
    if (d->field[DEFAULT]) puts_(&c.meta, "="), put_written(&c.meta, d->field[DEFAULT]);
    return c;
}

static size_t widest(size_t a, size_t b) { return a > b ? a : b; }

/* What cli-help answers: one line per declaration that has a flag, its long
   names, short names and meta:type=default each padded to its column's
   widest entry plus two, then its help, words wrapped to the width left of
   80 or 40 at least and continuation lines indented past the columns by two,
   as optparse's format_opt/5 lays it out; the empty text for none. NULL
   where the declarations are refused. */
static char *help_of(const mt_atom *spec)
{
    size_t n;
    decl *d = declared(spec, &n);
    if (!d) return NULL;
    columns *c = calloc(n + 1, sizeof *c);
    bool *shown = calloc(n + 1, sizeof *shown);
    size_t short_width = 0, meta_width = 0, longest = 0;
    require("room for the columns", c && shown);
    for (size_t i = 0; i < n; i++) {
        shown[i] = (d[i].field[SHORTS] && mt_len(d[i].field[SHORTS])) || (d[i].field[LONGS] && mt_len(d[i].field[LONGS]));
        if (!shown[i]) continue;
        c[i] = columns_of(&d[i]);
        short_width = widest(short_width, characters(c[i].shorts.s, c[i].shorts.n) + 2);
        meta_width = widest(meta_width, characters(c[i].meta.s, c[i].meta.n) + 2);
        for (size_t j = 0; j < c[i].n_longs; j++) longest = widest(longest, characters(c[i].longs[j].s, c[i].longs[j].n));
    }
    size_t long_width = longest + 2, indent = long_width + short_width + meta_width + 2;
    size_t width = indent + 40 > 80 ? 40 : 80 - indent;
    text help = { 0 };
    puts_(&help, "");
    char *continuation = malloc(indent + 2);
    require("room for an indent", continuation != NULL);
    continuation[0] = '\n', memset(continuation + 1, ' ', indent), continuation[indent + 1] = '\0';
    for (size_t i = 0; i < n; i++) {
        if (!shown[i]) continue;
        grouped(&help, c[i].longs, c[i].n_longs, longest, ", ", ",\n");
        pad_to(&help, long_width);
        put(&help, c[i].shorts.s, c[i].shorts.n);
        pad_to(&help, long_width + short_width);
        put(&help, c[i].meta.s, c[i].meta.n);
        pad_to(&help, long_width + short_width + meta_width);
        const mt_atom *lines = d[i].field[HELP];
        if (lines && mt_kind_of(lines) == MT_EXPR)
            for (size_t j = 0; j < mt_len(lines); j++) {
                if (j) puts_(&help, continuation);
                put(&help, mt_name(mt_at(lines, j)), mt_name_len(mt_at(lines, j)));
            }
        else if (lines) {
            token all = { mt_name(lines), mt_name_len(lines) }, *words = calloc(all.n + 1, sizeof *words);
            size_t count = 0;
            require("room for the words", words != NULL);
            for (const char *at = all.s, *end = all.s + all.n;; at++) {
                const char *space = memchr(at, ' ', (size_t)(end - at));
                words[count++] = (token){ at, (size_t)((space ? space : end) - at) };
                if (!space) break;
                at = space;
            }
            grouped(&help, words, count, width, " ", continuation);
            free(words);
        }
        puts_(&help, "\n");
    }
    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < c[i].n_longs; j++) free((char *)c[i].longs[j].s);
        free(c[i].longs), free(c[i].shorts.s), free(c[i].meta.s);
    }
    free(c), free(shown), free(continuation), free(d);
    return help.s;
}

static mt_atom *verdict(bool holds) { return S(holds ? "fine" : "refused"); }
static mt_answers *guarded(metta *m, mt_atom *goal) { return mt_eval(m, E("if-error", E("catch", goal), "refused", "fine")); }

/* An argument vector as MeTTa texts. */
static mt_atom *arguments(const token *tokens, size_t count)
{
    mt_atom **texts = malloc((count + 1) * sizeof *texts);
    require("room for the arguments", texts != NULL);
    for (size_t i = 0; i < count; i++) texts[i] = mt_textn(tokens[i].s, tokens[i].n);
    mt_atom *out = mt_exprv(count, texts);
    free(texts);
    return out;
}

/* The count declaration the help claims read, with or without its long
   name. */
static mt_atom *count_declaration(bool with_long)
{
    mt_atom *fields[] = { E("opt", "count"), E("type", "integer"), E("shortflags", E("n")), E("default", 1), E("meta", T("N")), E("help", T("Item count")),
                          with_long ? E("longflags", E("count")) : NULL };
    return E(mt_exprv(with_long ? 7 : 6, fields));
}

int main(int argc, char **argv)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_string", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_string")))));
    require("import lib_cli", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_cli")))));

    /* The application's declarations, cli-main over them, and cli-plus, the
       C function a custom type holds. */
    mt_atom *spec = E(E(E("opt", "count"), E("type", "integer"), E("shortflags", E("n")), E("longflags", E("count", "count2")), E("default", 1),
                        E("meta", T("N")), E("help", T("Item count"))),
                      E(E("opt", "verbose"), E("type", "boolean"), E("shortflags", E("v")), E("longflags", E("verbose")), E("default", B(false))),
                      E(E("opt", "name"), E("shortflags", E("s")), E("longflags", E("name")), E("default", T("guest"))));
    require("declare cli-main", mt_add(m, E(":", "cli-main", E("->", "Expression", "Symbol", "Expression"))));
    require("define cli-main", mt_add(m, E("=", E("cli-main", V("arguments"), V("duplicates")), E("cli-parse", mt_keep(spec), V("arguments"), V("duplicates")))));
    require("publish cli-plus", mt_def(m, (mt_op){ .name = "cli-plus", .arity = 2, .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = cli_plus }));

    mt_atom *types[KINDS];
    for (size_t k = 0; k < KINDS; k++) types[k] = S(kind_names[k]);
    assert(answers_are(mt_eval(m, E("cli-types")), E(mt_exprv(KINDS, types))) && "the built-in types");

    /* Every parse claim: the engine's answer against C's, a refusal where the
       original expects one. A row with no specification goes through
       cli-main, which holds the application's. */
    mt_atom *positive = E("Annotated", "Number", E("Gt", 0)), *value_row = E(E(E("opt", "value"), E("type", "metta"), E("longflags", E("value"))));
    mt_atom *sized = E(E(E("opt", "size"), E("type", E("parse", mt_keep(positive), "parse-number")), E("longflags", E("size")), E("default", 1)));
    const struct {
        const char *claim;
        mt_atom *spec;
        token args[6];
        size_t argc;
        const char *policy;
        bool refused;
    } rows[] = {
        { "defaults are typed", NULL, { { 0 } }, 0, "keeplast", false },
        { "a supplied count follows the defaults", NULL, { TOK("--count"), TOK("3"), TOK("file") }, 3, "keeplast", false },
        { "attached, equals and a bare Boolean", NULL, { TOK("-n4"), TOK("--name=π🙂"), TOK("-v"), TOK("input") }, 4, "keeplast", false },
        { "a second long name and a negation", NULL, { TOK("--count2=7"), TOK("--no-verbose") }, 2, "keepall", false },
        { "keepfirst keeps the first count", NULL, { TOK("-n"), TOK("2"), TOK("--name=a"), TOK("--count=3") }, 4, "keepfirst", false },
        { "keeplast keeps the last", NULL, { TOK("-n"), TOK("2"), TOK("--name=a"), TOK("--count=3") }, 4, "keeplast", false },
        { "keepall keeps both, in order", NULL, { TOK("-n"), TOK("2"), TOK("--name=a"), TOK("--count=3") }, 4, "keepall", false },
        { "three spellings of a Boolean", NULL, { TOK("--verbose=false"), TOK("-vtrue"), TOK("--no-verbose") }, 3, "keepall", false },
        { "a separate empty value", NULL, { TOK("--name"), TOK("") }, 2, "keeplast", false },
        { "an attached empty value", NULL, { TOK("--name=") }, 1, "keeplast", false },
        { "a dash-led value after =", NULL, { TOK("--name=-v") }, 1, "keeplast", false },
        { "a dash-led value attached", NULL, { TOK("-s--count") }, 1, "keeplast", false },
        { "a negative number is a value", NULL, { TOK("--count"), TOK("-3") }, 2, "keeplast", false },
        { "the terminator makes operands", NULL, { TOK("before"), TOK("--"), TOK("--count"), TOK("3"), TOK("") }, 5, "keeplast", false },
        { "a NUL inside a value", NULL, { TOK("--name=a\0b") }, 1, "keeplast", false },
        { "a value keeps its own =", NULL, { TOK("--name=a=b=c") }, 1, "keeplast", false },
        { "numbers, a lone dash and empty are data", mt_unit(), { TOK("-2"), TOK("-1.5"), TOK("-"), TOK("") }, 4, "keepall", false },
        { "one terminator is consumed", mt_unit(), { TOK("--"), TOK("--"), TOK("") }, 3, "keepall", false },
        { "no default, no pair", E(E(E("opt", "missing"), E("longflags", E("missing")))), { { 0 } }, 0, "keepall", false },
        { "an atom default and a text default", E(E(E("opt", "symbol"), E("type", "atom"), E("default", "_")), E(E("opt", "text"), E("default", T("_")))),
          { { 0 } }, 0, "keepall", false },
        { "short and long names are separate", E(E(E("opt", "short"), E("shortflags", E("x"))), E(E("opt", "long"), E("longflags", E("x")))),
          { TOK("-x"), TOK("left"), TOK("--x"), TOK("right") }, 4, "keepall", false },
        { "a digit as a short name", E(E(E("opt", "digit"), E("type", "integer"), E("shortflags", E(T("2"))))), { TOK("-2"), TOK("7") }, 2, "keepall", false },
        { "punctuation in a long name", E(E(E("opt", "mark"), E("longflags", E("9?")))), { TOK("--9?=yes") }, 1, "keepall", false },
        { "a declared --no-name wins", E(E(E("opt", "verbose"), E("type", "boolean"), E("longflags", E("verbose"))), E(E("opt", "named"), E("longflags", E("no-verbose")))),
          { TOK("--no-verbose"), TOK("literal") }, 2, "keepall", false },
        { "a float", E(E(E("opt", "ratio"), E("type", "float"), E("longflags", E("ratio")))), { TOK("--ratio=1.25") }, 1, "keepall", false },
        { "an atom", E(E(E("opt", "mode"), E("type", "atom"), E("longflags", E("mode")))), { TOK("--mode=scan") }, 1, "keepall", false },
        { "a form read, not run", mt_keep(value_row), { TOK("--value=(+ 1 2)") }, 1, "keepall", false },
        { "the empty form", mt_keep(value_row), { TOK("--value=()") }, 1, "keepall", false },
        { "a form keeps its sharing", mt_keep(value_row), { TOK("--value=(row $x $x)") }, 1, "keepall", false },
        { "a refined custom type", mt_keep(sized), { TOK("--size=3") }, 1, "keepall", false },
        { "its default", mt_keep(sized), { { 0 } }, 0, "keepall", false },
        { "a lambda converter", E(E(E("opt", "text"), E("type", E("parse", "String", lambda(E("string-upper", V("x"))))), E("longflags", E("text")))),
          { TOK("--text=hello") }, 1, "keepall", false },
        { "a partial application converter", E(E(E("opt", "size"), E("type", E("parse", "Number", E("cli-plus", 10))), E("longflags", E("size")))),
          { TOK("--size=5") }, 1, "keepall", false },
        { "a missing value", NULL, { TOK("--name") }, 1, "keeplast", true },
        { "a flag is no value", NULL, { TOK("--name"), TOK("--verbose") }, 2, "keeplast", true },
        { "an unknown name", NULL, { TOK("--unknown") }, 1, "keeplast", true },
        { "a name never declared", NULL, { TOK("--bad?") }, 1, "keeplast", true },
        { "-n=value", NULL, { TOK("-n=3") }, 1, "keeplast", true },
        { "short names do not cluster", NULL, { TOK("-vn") }, 1, "keeplast", true },
        { "a bad value cannot hide behind a later one", NULL, { TOK("--count=bad"), TOK("--count=3") }, 2, "keeplast", true },
        { "an unknown policy", mt_unit(), { { 0 } }, 0, "unknown", true },
        { "a key twice", E(E(E("opt", "x")), E(E("opt", "x"))), { { 0 } }, 0, "keepall", true },
        { "a field twice", E(E(E("opt", "x"), E("type", "integer"), E("type", "integer"))), { { 0 } }, 0, "keepall", true },
        { "a long name twice", E(E(E("opt", "x"), E("longflags", E("same"))), E(E("opt", "y"), E("longflags", E("same")))), { { 0 } }, 0, "keepall", true },
        { "a name with a space", E(E(E("opt", "x"), E("longflags", E(T("bad name"))))), { { 0 } }, 0, "keepall", true },
        { "an unknown field", E(E(E("opt", "x"), E("wat", 1))), { { 0 } }, 0, "keepall", true },
        { "no opt", E(E(E("type", "string"))), { { 0 } }, 0, "keepall", true },
        { "an unknown type", E(E(E("opt", "x"), E("type", "unknown"))), { { 0 } }, 0, "keepall", true },
        { "a default of the wrong type", E(E(E("opt", "x"), E("type", "integer"), E("default", T("bad")))), { { 0 } }, 0, "keepall", true },
        { "two forms are not one", E(E(E("opt", "x"), E("type", "metta"), E("longflags", E("x")))), { TOK("--x=1 2") }, 1, "keepall", true },
        { "a refinement refuses 0", E(E(E("opt", "x"), E("type", E("parse", mt_keep(positive), "parse-number")), E("longflags", E("x")))),
          { TOK("--x=0") }, 1, "keepall", true },
        { "a converter with no answer", E(E(E("opt", "x"), E("type", E("parse", "String", lambda(E("empty")))), E("longflags", E("x")))),
          { TOK("--x=a") }, 1, "keepall", true },
        { "a converter with two", E(E(E("opt", "x"), E("type", E("parse", "Number", lambda(E("superpose", E(1, 2))))), E("longflags", E("x")))),
          { TOK("--x=a") }, 1, "keepall", true },
        { "a converter of the wrong type", E(E(E("opt", "x"), E("type", E("parse", "Number", lambda(T("wrong")))), E("longflags", E("x")))),
          { TOK("--x=a") }, 1, "keepall", true },
    };
    for (size_t i = 0; i < sizeof rows / sizeof *rows; i++) {
        mt_atom *args = arguments(rows[i].args, rows[i].argc), *policy = S(rows[i].policy);
        mt_atom *goal = rows[i].spec ? E("cli-parse", mt_keep(rows[i].spec), mt_keep(args), policy) : E("cli-main", mt_keep(args), policy);
        refusal why;
        mt_atom *c_answer = parsed(rows[i].spec ? rows[i].spec : spec, args, rows[i].policy, &why);
        if (rows[i].refused) assert(answers_are(guarded(m, goal), E(verdict(c_answer != NULL))) && rows[i].claim), mt_drop(c_answer);
        else assert(answers_are(mt_eval(m, goal), E(c_answer)) && rows[i].claim);
        mt_drop(args);
        if (rows[i].spec) mt_drop(rows[i].spec);
    }
    const token nine[] = { TOK("--count=9") };
    mt_atom *held_args = arguments(nine, 1);
    refusal why;
    assert(answers_are(mt_eval(m, E("let", V("arguments"), E("quote", mt_keep(held_args)), E("cli-main", V("arguments"), "keeplast"))), E(parsed(spec, held_args, "keeplast", &why)))
           && "a held argument vector");
    token *own = calloc((size_t)argc + 1, sizeof *own);
    require("room for argv", own != NULL);
    for (int i = 0; i < argc; i++) own[i] = (token){ argv[i], strlen(argv[i]) };
    mt_atom *process = arguments(own, (size_t)argc);
    assert(answers_are(mt_eval(m, E("let", V("arguments"), E("cli-arguments!"), E("get-metatype", V("arguments")))), E(S(metatype(process))))
           && "the process's arguments are an expression");
    free(own);

    /* Help derives from declarations: C lays it out and the engine agrees,
       and each fact the original looks for is in C's layout. */
    const struct { const char *claim; mt_atom *spec; const char *needle; } helps[] = {
        { "no declarations, no help", mt_unit(), NULL },
        { "a flagless declaration is not shown", E(E(E("opt", "hidden"), E("default", T("local")))), NULL },
        { "the help names --count", count_declaration(true), "--count" },
        { "and N:integer=1", count_declaration(false), "N:integer=1" },
        { "every help line", E(E(E("opt", "text"), E("longflags", E("text")), E("default", T("π")), E("help", E(T("First line"), T("Second line"))))), "Second line" },
        { "a custom type's label", E(E(E("opt", "text"), E("type", E("parse", "String", lambda(E("empty")))), E("longflags", E("text")))), "String" },
    };
    for (size_t i = 0; i < sizeof helps / sizeof *helps; i++) {
        char *c_help = help_of(helps[i].spec);
        require("C lays out the help", c_help != NULL);
        if (helps[i].needle)
            assert(answers_are(mt_eval(m, E("string-contains", E("cli-help", mt_keep(helps[i].spec)), T(helps[i].needle))), E(B(strstr(c_help, helps[i].needle) != NULL)))
                   && helps[i].claim);
        assert(answers_are(mt_eval(m, E("cli-help", mt_keep(helps[i].spec))), E(T(c_help))) && (helps[i].needle ? "laid out as C lays it out" : helps[i].claim));
        free(c_help);
        mt_drop(helps[i].spec);
    }

    /* A refusal names the option and keeps its cause. */
    const token bad[] = { TOK("--count"), TOK("bad") };
    mt_atom *bad_args = arguments(bad, 2), *c_bad = parsed(spec, bad_args, "keeplast", &why);
    require("C refuses it", c_bad == NULL && why.why != NULL);
    mt_atom *error = mt_one(mt_eval(m, E("catch", E("cli-main", mt_keep(bad_args), "keeplast"))));
    require("an error", error && headed(error, "Error") && mt_len(error) == 3);
    char *ball = mt_show_dup(mt_at(error, 1)), *context = mt_show_dup(mt_at(error, 2)), *flag_text = strndup(why.at.s, why.at.n);
    require("the error's words", ball && context && flag_text);
    assert(strstr(ball, flag_text) != NULL && "the refusal names the option C refused at");
    assert(strstr(why.why, "declared type") && strstr(context, "declared type") && "and the declared type C's refusal asks for");
    mt_free(ball), mt_free(context), free(flag_text);

    mt_atom *held[] = { spec, positive, value_row, sized, held_args, process, bad_args, error };
    for (size_t i = 0; i < sizeof held / sizeof *held; i++) mt_drop(held[i]);
    mt_close(m);
    return 0;
}
#else
#include <stdio.h>

/* Without GMP and utf8proc's headers the program only says what it needs. */
int main(void)
{
    fputs("44-cli_lib.c needs GMP and utf8proc: install its development files, then build with\n"
          "cc 44-cli_lib.c $(pkg-config --cflags --libs cmetta gmp libutf8proc)\n", stderr);
    return 77;
}
#endif
