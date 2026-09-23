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
 * Guarantees: all twenty-two claims of the original hold [tested: make
 *   twins; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"
#include <unistd.h>

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
    metta *m = open_engine();
    require("import lib_csv", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_csv")))));

    const char *two = "001,\"a,b\"\r\n002,9\r\n";
    static const char *const r1[] = { "001", "a,b" }, *const r2[] = { "002", "9" };
    static const row both[] = { r1, r2 }, again[] = { r2 };
    static const size_t pairs[] = { 2, 2 };
    check_answers("parse", mt_eval(m, E("csv-parse", T(two))), parsed(two, DEFAULTS));
    check_answers("encode", mt_eval(m, E("csv-encode", table(both, pairs, 2))), encoded(both, pairs, 2, DEFAULTS));
    const csv semicolon_skip = { ";", "\"", "\r\n", 1 }, semicolon_lf = { ";", "\"", "\n", 0 };
    const char *headed = "id;value\n001;a\n001;a\n";
    check_answers("a separator and a header", mt_eval(m, E("csv-parse", T(headed), options(semicolon_skip, false))),
                  parsed(headed, semicolon_skip));
    static const char *const r3[] = { "001", "a;b" };
    static const row one[] = { r3 };
    check_answers("encoding with a separator and a newline",
                  mt_eval(m, E("csv-encode", table(one, pairs, 1), options(semicolon_lf, true))),
                  encoded(one, pairs, 1, semicolon_lf));
    check_answers("no text, no records", mt_eval(m, E("csv-parse", T(""))), parsed("", DEFAULTS));
    check_answers("no records, no text", mt_eval(m, E("csv-encode", mt_unit())), encoded(NULL, NULL, 0, DEFAULTS));
    const char *ragged = "\n\"\"\n,\n";
    check_answers("rows of any width", mt_eval(m, E("csv-parse", T(ragged), E("quote", E(E("width", "any"))))),
                  parsed(ragged, DEFAULTS));
    const csv unquoted = { ",", "", "\r\n", 0 };
    const char *literal = "a\"b,c\n";
    check_answers("no quote character", mt_eval(m, E("csv-parse", T(literal), options(unquoted, false))),
                  parsed(literal, unquoted));
    const csv fox = { "🦊", "λ", "\r\n", 0 };
    static const char *const r4[] = { "é🦊", "λ\r\n" };
    static const row odd[] = { r4 };
    mt_atom *text = encoded(odd, pairs, 1, fox);
    check_answers("a multibyte separator and quote round-trip",
                  mt_eval(m, E("csv-parse", E("csv-encode", table(odd, pairs, 1), options(fox, false)), options(fox, false))),
                  parsed(mt_name(text), fox));
    mt_drop(text);

    /* Files. */
    require("import lib_file", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_file")))));
    mt_atom *dir = value_of(m, E("temp-dir!", T("metta-csv")));
    mt_atom *path = value_of(m, E("path-join", mt_keep(dir), T("records.csv")));
    check_answers("csv-write!", mt_eval(m, E("csv-write!", mt_keep(path), table(both, pairs, 2))), B(true));
    check_answers("csv-read! answers what C reads on disk", mt_eval(m, E("collapse", E("csv-read!", mt_keep(path)))),
                  on_disk(path, DEFAULTS, false));
    check_answers("csv-append!", mt_eval(m, E("csv-append!", mt_keep(path), table(again, pairs, 1))), B(true));
    check_answers("three records now", mt_eval(m, E("collapse", E("csv-read!", mt_keep(path)))),
                  on_disk(path, DEFAULTS, false));
    mt_atom *live = value_of(m, E("csv-space", mt_keep(path)));
    check_answers("a live space holds the rows", rows_of(m, live, false), on_disk(path, DEFAULTS, false));
    mt_atom *shot = value_of(m, E("csv-snapshot!", mt_keep(path))), *first_shot = on_disk(path, DEFAULTS, true);
    check_answers("a snapshot numbers its records", rows_of(m, shot, true), mt_keep(first_shot));

    const csv semicolon = { ";", "\"", "\r\n", 0 };
    static const char *const header[] = { "id", "value" }, *const r5[] = { "003", "a;b" }, *const r6[] = { "004", "7" },
                             *const r7[] = { "005", "later" };
    static const row rewritten[] = { header, r5 }, added[] = { r6 }, later[] = { header, r7 };
    check_answers("rewrite with a separator",
                  mt_eval(m, E("csv-write!", mt_keep(path), table(rewritten, pairs, 2), options(semicolon, false))), B(true));
    check_answers("append with it", mt_eval(m, E("csv-append!", mt_keep(path), table(added, pairs, 1), options(semicolon, false))),
                  B(true));
    check_answers("read past the header",
                  mt_eval(m, E("collapse", E("csv-read!", mt_keep(path), options(semicolon_skip, false)))),
                  on_disk(path, semicolon_skip, false));
    mt_atom *configured = value_of(m, E("csv-space", mt_keep(path), options(semicolon_skip, false)));
    check_answers("a configured live space", rows_of(m, configured, false), on_disk(path, semicolon_skip, false));
    mt_atom *numbered = value_of(m, E("csv-snapshot!", mt_keep(path), options(semicolon_skip, false)));
    check_answers("numbers count the skipped header", rows_of(m, numbered, true), on_disk(path, semicolon_skip, true));

    /* A snapshot keeps what it read; a live space follows the file. */
    check_answers("the first snapshot is unchanged", rows_of(m, shot, true), first_shot);
    require("the file is rewritten",
            mt_one_truth(mt_eval(m, E("csv-write!", mt_keep(path), table(later, pairs, 2), options(semicolon, false)))));
    check_answers("the live space reads the file again", rows_of(m, configured, false), on_disk(path, semicolon_skip, false));

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
    return done(m);
}
