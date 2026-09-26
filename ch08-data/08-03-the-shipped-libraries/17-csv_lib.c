/* Purpose: lib_csv, held against a CSV reader and writer written in C over
 *   bytes: RFC 4180 with any separator and quote, each a UTF-8 byte string
 *   matched where it starts. A record ends at LF or CRLF, a quoted field runs
 *   to a quote that is not doubled, a doubled quote stands for one, and an
 *   empty quote string quotes nothing. Writing quotes a field holding the
 *   separator, the quote or a line break, doubles its quotes, and ends each
 *   record with the newline. The file doors are checked against C parsing
 *   the bytes on disk, and a snapshot's row numbers against C counting
 *   records, skipped ones included, so a header is skipped by position. The
 *   original names its directory, path and spaces with bind!, a token of its
 *   reader; C holds the values the engine answers in variables, and deletes
 *   the file, its writer's lock and the directory itself.
 * Guarantees: all twenty-two claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

enum { MOST = 16, FIELD = 64, TEXT = 8 * FIELD };

typedef struct csv {
    const char *separator, *quote, *newline;
    size_t skip;
} csv;

static const csv DEFAULTS = { ",", "\"", "\r\n", 0 };

static bool at(const char *text, size_t pos, size_t len, const char *what)
{
    size_t n = strlen(what);
    return n > 0 && pos + n <= len && memcmp(text + pos, what, n) == 0;
}

static size_t line_end(const char *text, size_t pos, size_t len)
{
    if (at(text, pos, len, "\r\n")) return 2;
    return pos < len && text[pos] == '\n';
}

/* The records of the text after the first o.skip, each the expression of its
   fields, led by its record number when numbered.
   Time: one pass over len bytes, a strlen of separator and quote per byte. */
static mt_atom *records(const char *text, size_t len, csv o, bool numbered)
{
    mt_atom *rows[MOST];
    size_t n_rows = 0, number = 0, pos = 0;
    while (pos < len) {
        mt_atom *fields[MOST];
        size_t n = 0, ended = line_end(text, pos, len);
        if (++number, numbered) fields[n++] = mt_num((int64_t)number);
        while (!ended) {
            char field[FIELD];
            size_t w = 0;
            if (at(text, pos, len, o.quote)) {
                for (pos += strlen(o.quote);; field[w++] = text[pos++]) {
                    require("a quoted field closes", pos < len && w < FIELD);
                    if (at(text, pos, len, o.quote) && !at(text, pos += strlen(o.quote), len, o.quote)) break;
                }
            } else
                while (pos < len && !at(text, pos, len, o.separator) && !line_end(text, pos, len)) {
                    require("room for the field", w < FIELD);
                    field[w++] = text[pos++];
                }
            require("room for the record", n < MOST);
            fields[n++] = mt_textn(field, w);
            if (!at(text, pos, len, o.separator)) {
                ended = line_end(text, pos, len);
                require("a record ends at a line break", ended || pos == len);
                break;
            }
            pos += strlen(o.separator);
        }
        pos += ended;
        if (number <= o.skip) {
            while (n) mt_drop(fields[--n]);
            continue;
        }
        require("room for the rows", n_rows < MOST);
        rows[n_rows++] = mt_exprv(n, fields);
    }
    return mt_exprv(n_rows, rows);
}

static mt_atom *parsed(const char *text, csv o) { return records(text, strlen(text), o, false); }

static void put(char *out, const char *s, size_t n)
{
    size_t used = strlen(out);
    require("room for the text", used + n < TEXT);
    memcpy(out + used, s, n);
    out[used + n] = '\0';
}

typedef const char *const *row;

/* The rows as CSV text, a field quoted when it holds the separator, the
   quote or a line break. */
static mt_atom *encoded(const row *rows, const size_t *widths, size_t n, csv o)
{
    char out[TEXT] = "";
    size_t q = strlen(o.quote);
    for (size_t r = 0; r < n; r++) {
        for (size_t f = 0; f < widths[r]; f++) {
            const char *s = rows[r][f];
            bool quoted = q && (strstr(s, o.separator) || strstr(s, o.quote) || strpbrk(s, "\r\n"));
            if (f) put(out, o.separator, strlen(o.separator));
            if (quoted) put(out, o.quote, q);
            for (const char *quote; quoted && (quote = strstr(s, o.quote)); s = quote + q) {
                put(out, s, (size_t)(quote - s) + q);
                put(out, o.quote, q);
            }
            put(out, s, strlen(s));
            if (quoted) put(out, o.quote, q);
        }
        put(out, o.newline, strlen(o.newline));
    }
    return mt_text(out);
}

/* Rows as the engine takes them: an expression of expressions of texts. */
static mt_atom *table(const row *rows, const size_t *widths, size_t n)
{
    mt_atom *out[MOST];
    for (size_t r = 0; r < n; r++) {
        mt_atom *fields[MOST];
        for (size_t f = 0; f < widths[r]; f++) fields[f] = mt_text(rows[r][f]);
        out[r] = mt_exprv(widths[r], fields);
    }
    return mt_exprv(n, out);
}

/* Options as the engine takes them, quoted so that an option named quote is
   data, not a call. */
static mt_atom *options(csv o, bool newline)
{
    mt_atom *pairs[4];
    size_t n = 0;
    if (strcmp(o.separator, DEFAULTS.separator) != 0) pairs[n++] = E("separator", T(o.separator));
    if (strcmp(o.quote, DEFAULTS.quote) != 0) pairs[n++] = E("quote", T(o.quote));
    if (newline) pairs[n++] = E("newline", T(o.newline));
    if (o.skip) pairs[n++] = E("skip", (int64_t)o.skip);
    return E("quote", mt_exprv(n, pairs));
}

/* The file's records as C reads its bytes. */
static mt_atom *on_disk(const mt_atom *path, csv o, bool numbered)
{
    char data[TEXT];
    FILE *f = fopen(mt_name(path), "rb");
    require("C opens what the engine wrote", f != NULL);
    size_t n = fread(data, 1, sizeof data, f);
    fclose(f);
    require("C reads all of it", n < sizeof data);
    return records(data, n, o, numbered);
}

static mt_atom *value_of(metta *m, mt_atom *goal)
{
    mt_atom *v = mt_one(mt_eval(m, goal));
    require("a value", v != NULL);
    return v;
}

/* What a space holds under (row ...), each row as its fields, numbered ones
   led by the number. */
static mt_answers *rows_of(metta *m, const mt_atom *space, bool numbered)
{
    mt_atom *pattern = numbered ? E("row", V("n"), V("id"), V("value")) : E("row", V("id"), V("value"));
    mt_atom *shape = numbered ? E(V("n"), V("id"), V("value")) : E(V("id"), V("value"));
    return mt_eval(m, E("collapse", E("match", mt_keep(space), pattern, shape)));
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_csv", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_csv")))));

    const char *two = "001,\"a,b\"\r\n002,9\r\n";
    static const char *const r1[] = { "001", "a,b" }, *const r2[] = { "002", "9" };
    static const row both[] = { r1, r2 }, again[] = { r2 };
    static const size_t pairs[] = { 2, 2 };
    assert(answers_are(mt_eval(m, E("csv-parse", T(two))), E(parsed(two, DEFAULTS))) && "parse");
    assert(answers_are(mt_eval(m, E("csv-encode", table(both, pairs, 2))), E(encoded(both, pairs, 2, DEFAULTS))) && "encode");
    const csv semicolon_skip = { ";", "\"", "\r\n", 1 }, semicolon_lf = { ";", "\"", "\n", 0 };
    const char *headed = "id;value\n001;a\n001;a\n";
    assert(answers_are(mt_eval(m, E("csv-parse", T(headed), options(semicolon_skip, false))), E(parsed(headed, semicolon_skip)))
           && "a separator and a header");
    static const char *const r3[] = { "001", "a;b" };
    static const row one[] = { r3 };
    assert(answers_are(mt_eval(m, E("csv-encode", table(one, pairs, 1), options(semicolon_lf, true))), E(encoded(one, pairs, 1, semicolon_lf)))
           && "encoding with a separator and a newline");
    assert(answers_are(mt_eval(m, E("csv-parse", T(""))), E(parsed("", DEFAULTS))) && "no text, no records");
    assert(answers_are(mt_eval(m, E("csv-encode", mt_unit())), E(encoded(NULL, NULL, 0, DEFAULTS))) && "no records, no text");
    const char *ragged = "\n\"\"\n,\n";
    assert(answers_are(mt_eval(m, E("csv-parse", T(ragged), E("quote", E(E("width", "any"))))), E(parsed(ragged, DEFAULTS)))
           && "rows of any width");
    const csv unquoted = { ",", "", "\r\n", 0 };
    const char *literal = "a\"b,c\n";
    assert(answers_are(mt_eval(m, E("csv-parse", T(literal), options(unquoted, false))), E(parsed(literal, unquoted)))
           && "no quote character");
    const csv fox = { "🦊", "λ", "\r\n", 0 };
    static const char *const r4[] = { "é🦊", "λ\r\n" };
    static const row odd[] = { r4 };
    mt_atom *text = encoded(odd, pairs, 1, fox);
    assert(answers_are(mt_eval(m, E("csv-parse", E("csv-encode", table(odd, pairs, 1), options(fox, false)), options(fox, false))), E(parsed(mt_name(text), fox)))
           && "a multibyte separator and quote round-trip");
    mt_drop(text);

    /* Files. */
    require("import lib_file", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_file")))));
    mt_atom *dir = value_of(m, E("temp-dir!", T("metta-csv")));
    mt_atom *path = value_of(m, E("path-join", mt_keep(dir), T("records.csv")));
    assert(answers_are(mt_eval(m, E("csv-write!", mt_keep(path), table(both, pairs, 2))), E(B(true))) && "csv-write!");
    assert(answers_are(mt_eval(m, E("collapse", E("csv-read!", mt_keep(path)))), E(on_disk(path, DEFAULTS, false)))
           && "csv-read! answers what C reads on disk");
    assert(answers_are(mt_eval(m, E("csv-append!", mt_keep(path), table(again, pairs, 1))), E(B(true))) && "csv-append!");
    assert(answers_are(mt_eval(m, E("collapse", E("csv-read!", mt_keep(path)))), E(on_disk(path, DEFAULTS, false)))
           && "three records now");
    mt_atom *live = value_of(m, E("csv-space", mt_keep(path)));
    assert(answers_are(rows_of(m, live, false), E(on_disk(path, DEFAULTS, false))) && "a live space holds the rows");
    mt_atom *shot = value_of(m, E("csv-snapshot!", mt_keep(path))), *first_shot = on_disk(path, DEFAULTS, true);
    assert(answers_are(rows_of(m, shot, true), E(mt_keep(first_shot))) && "a snapshot numbers its records");

    const csv semicolon = { ";", "\"", "\r\n", 0 };
    static const char *const header[] = { "id", "value" }, *const r5[] = { "003", "a;b" }, *const r6[] = { "004", "7" },
                             *const r7[] = { "005", "later" };
    static const row rewritten[] = { header, r5 }, added[] = { r6 }, later[] = { header, r7 };
    assert(answers_are(mt_eval(m, E("csv-write!", mt_keep(path), table(rewritten, pairs, 2), options(semicolon, false))), E(B(true)))
           && "rewrite with a separator");
    assert(answers_are(mt_eval(m, E("csv-append!", mt_keep(path), table(added, pairs, 1), options(semicolon, false))), E(B(true)))
           && "append with it");
    assert(answers_are(mt_eval(m, E("collapse", E("csv-read!", mt_keep(path), options(semicolon_skip, false)))), E(on_disk(path, semicolon_skip, false)))
           && "read past the header");
    mt_atom *configured = value_of(m, E("csv-space", mt_keep(path), options(semicolon_skip, false)));
    assert(answers_are(rows_of(m, configured, false), E(on_disk(path, semicolon_skip, false))) && "a configured live space");
    mt_atom *numbered = value_of(m, E("csv-snapshot!", mt_keep(path), options(semicolon_skip, false)));
    assert(answers_are(rows_of(m, numbered, true), E(on_disk(path, semicolon_skip, true))) && "numbers count the skipped header");

    /* A snapshot keeps what it read; a live space follows the file. */
    assert(answers_are(rows_of(m, shot, true), E(first_shot)) && "the first snapshot is unchanged");
    require("the file is rewritten",
            mt_one_truth(mt_eval(m, E("csv-write!", mt_keep(path), table(later, pairs, 2), options(semicolon, false)))));
    assert(answers_are(rows_of(m, configured, false), E(on_disk(path, semicolon_skip, false))) && "the live space reads the file again");

    char lock[512];
    snprintf(lock, sizeof lock, "%s.metta-csv.lock", mt_name(path));
    require("the records go", remove(mt_name(path)) == 0);
    require("the writer's lock goes", remove(lock) == 0);
    require("the directory goes", rmdir(mt_name(dir)) == 0);
    mt_drop(live);
    mt_drop(shot);
    mt_drop(configured);
    mt_drop(numbered);
    mt_drop(path);
    mt_drop(dir);
    mt_close(m);
    return 0;
}
