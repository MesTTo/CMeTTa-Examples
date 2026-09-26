/* Purpose: lib_json, held against cJSON, a JSON parser C programs link.
 *   C parses the text the engine decodes and reads its answer off cJSON's tree:
 *   an array is an expression, a scalar itself, an object's keys and values
 *   are what the decoded space holds, and a path follows every field of a
 *   repeated name. What the engine encodes C parses back, a JSON Lines record
 *   is a value cJSON prints ended by LF, and a file the engine writes C reads
 *   from disk and parses. The two pretty layouts are the native writer's own
 *   and stay literal, each read by cJSON as the same list.
 * Build: cc 05-json_lib.c $(pkg-config --cflags --libs cmetta libcjson)
 * Assumes: libcjson, found through pkg-config.
 * Guarantees: all twenty-eight claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define _POSIX_C_SOURCE 200809L
#define MT_SHORTHAND
#if __has_include(<cjson/cJSON.h>)
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>

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

/* Whether an atom is want up to renaming variables; takes both, and shows
   them when they differ. */
static inline bool atom_is(mt_atom *got, mt_atom *want)
{
    bool holds = got && want && mt_alpha_eq(got, want);
    if (!holds) fprintf(stderr, "  got %s\n  want %s\n", got ? mt_show(got) : "nothing",
                        want ? mt_show(want) : "nothing");
    mt_drop(got);
    mt_drop(want);
    return holds;
}

enum { MOST = 16 };

/* A cJSON value as the atom lib_json decodes it to, for everything but an
   object, which decodes to a space: an array is an expression, a string text,
   a boolean True or False and null the symbol Null. cJSON keeps one double
   per number, so C reads an integral one as the integer JSON wrote, which
   every number here is. */
static mt_atom *atom_of(const cJSON *j)
{
    if (cJSON_IsArray(j)) {
        mt_atom *kids[MOST];
        size_t n = 0;
        const cJSON *item;
        cJSON_ArrayForEach(item, j) kids[n++] = atom_of(item);
        return mt_exprv(n, kids);
    }
    if (cJSON_IsString(j)) return mt_text(j->valuestring);
    if (cJSON_IsBool(j)) return mt_bool(cJSON_IsTrue(j));
    if (cJSON_IsNull(j)) return mt_sym("Null");
    require("a number", cJSON_IsNumber(j));
    double d = j->valuedouble;
    return d == (double)(int64_t)d ? mt_num((int64_t)d) : mt_real(d);
}

static cJSON *parsed(const char *text)
{
    cJSON *j = cJSON_Parse(text);
    require("cJSON parses the text", j != NULL);
    return j;
}

static mt_atom *read_json(const char *text)
{
    cJSON *j = parsed(text);
    mt_atom *a = atom_of(j);
    cJSON_Delete(j);
    return a;
}

/* The answers a path reaches: each step an object field, every field of that
   name when the name repeats, or a zero-based array index. */
typedef struct answers { size_t n; mt_atom *item[MOST]; } answers;

static void walk(const cJSON *j, const char *const *path, size_t steps, answers *out)
{
    if (!steps) {
        out->item[out->n++] = atom_of(j);
        return;
    }
    if (cJSON_IsArray(j)) {
        const cJSON *item = cJSON_GetArrayItem(j, atoi(*path));
        if (item) walk(item, path + 1, steps - 1, out);
        return;
    }
    const cJSON *field;
    cJSON_ArrayForEach(field, j)
        if (field->string && strcmp(field->string, *path) == 0) walk(field, path + 1, steps - 1, out);
}

static answers at(const char *text, size_t steps, const char *const *path)
{
    answers out = { 0 };
    cJSON *j = parsed(text);
    walk(j, path, steps, &out);
    cJSON_Delete(j);
    return out;
}

static answers keys(const char *text)
{
    answers out = { 0 };
    cJSON *j = parsed(text);
    const cJSON *field;
    cJSON_ArrayForEach(field, j) out.item[out.n++] = mt_sym(field->string);
    cJSON_Delete(j);
    return out;
}

/* JSON Lines: every physical line one value, "\r\n" ending a line as "\n"
   does, since a CR is JSON whitespace. */
static answers lines(const char *text)
{
    answers out = { 0 };
    char copy[8 * MOST];
    snprintf(copy, sizeof copy, "%s", text);
    for (char *line = strtok(copy, "\n"); line; line = strtok(NULL, "\n"))
        out.item[out.n++] = read_json(line);
    return out;
}

/* A value's JSON Lines record, cJSON's compact print ended by LF. */
static void record(char *out, size_t size, cJSON *value)
{
    char *text = cJSON_PrintUnformatted(value);
    size_t used = strlen(out);
    snprintf(out + used, size - used, "%s\n", text);
    cJSON_free(text);
    cJSON_Delete(value);
}

static char *slurp(const char *path)
{
    FILE *f = fopen(path, "rb");
    char *data = f ? calloc(1, 4096) : NULL;
    if (data) data[fread(data, 1, 4095, f)] = '\0';
    if (f) fclose(f);
    require("C reads what the engine wrote", data != NULL);
    return data;
}

static void check_all(const char *claim, mt_answers *got, answers want)
{
    assert(answers_are(got, mt_exprv(want.n, want.item)) && claim);
}

static mt_atom *decode(const char *text) { return E("json-decode", T(text)); }

/* The text the engine answers for an expression. */
static char *engine_text(metta *m, mt_atom *goal)
{
    mt_atom *answer = mt_one(mt_eval(m, goal));
    require("a text answer", answer && mt_kind_of(answer) == MT_TEXT);
    char *copy = strdup(mt_name(answer));
    mt_drop(answer);
    return copy;
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_json", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_json")))));

    /* An object decodes to a space whose atoms are its fields. */
    const char *pair = "{\"a\":1,\"b\":2}";
    check_all("get-keys answers the object's keys", mt_eval(m, E("let", V("d"), decode(pair), E("get-keys", V("d")))),
              keys(pair));
    const char *const a[] = { "a" }, *const missing[] = { "missing" };
    check_all("get-value answers a field", mt_eval(m, E("let", V("d"), decode(pair), E("get-value", V("d"), "a"))),
              at(pair, 1, a));
    assert(!mt_first(mt_eval(m, E("let", V("d"), decode("{\"a\":1}"), E("get-value", V("d"), "missing")))) && mt_ok()
           && "an absent key answers nothing");

    const char *const scalars[] = { "[1,2,3]", "\"plain\"", "42", "true", "null" };
    for (size_t i = 0; i < sizeof scalars / sizeof *scalars; i++)
        assert(answers_are(mt_eval(m, decode(scalars[i])), E(read_json(scalars[i]))) && scalars[i]);

    const char *nested = "{\"c\":{\"d\":2}}";
    const char *const c_d[] = { "c", "d" };
    check_all("an inner object is a space too",
              mt_eval(m, E("let", V("outer"), decode(nested),
                           E("let", V("inner"), E("get-value", V("outer"), "c"), E("get-value", V("inner"), "d")))),
              at(nested, 2, c_d));

    /* Encoding inverts decoding, and cJSON reads the engine's text as the
       value it encoded. */
    char *list = engine_text(m, E("json-encode", E(1, 2, 3)));
    assert(atom_is(read_json(list), E(1, 2, 3)) && "cJSON reads the encoded list as the list");
    assert(answers_are(mt_eval(m, decode(list)), E(read_json(list))) && "encoding inverts decoding");
    char *text = engine_text(m, E("json-encode", T("text")));
    assert(atom_is(read_json(text), T("text")) && "and a text");
    assert(answers_are(mt_eval(m, decode(text)), E(read_json(text))) && "for text too");
    char *object = engine_text(m, E("json-encode", E("dict-space", E(E("k", 1)))));
    const char *const k[] = { "k" };
    check_all("and for an object built from pairs",
              mt_eval(m, E("let", V("d"), decode(object), E("get-value", V("d"), "k"))), at(object, 1, k));
    free(list);
    free(text);
    free(object);

    /* dict-space holds the pairs C holds, so a lookup is C's own. */
    static const struct { const char *key; const char *text; int64_t number; } person[] = {
        { "name", "ann", 0 }, { "age", NULL, 3 } };
    mt_atom *rows[2];
    for (size_t i = 0; i < 2; i++)
        rows[i] = E(mt_sym(person[i].key), person[i].text ? T(person[i].text) : mt_num(person[i].number));
    mt_atom *found = NULL;
    for (size_t i = 0; i < 2 && !found; i++)
        if (strcmp(person[i].key, "name") == 0) found = mt_keep(mt_at(rows[i], 1));
    assert(answers_are(mt_eval(m, E("let", V("d"), E("dict-space", mt_exprv(2, rows)), E("get-value", V("d"), "name"))), E(found))
           && "dict-space builds one from pairs");

    /* Paths mix field names with zero-based indexes. */
    const char *table = "{\"rows\":[{\"name\":\"ann\"}]}", *twice = "{\"a\":[1],\"a\":[2]}";
    const char *const rows_0_name[] = { "rows", "0", "name" }, *const a_0[] = { "a", "0" };
    check_all("a path through an array", mt_eval(m, E("json-at", decode(table), E("rows", 0, "name"))),
              at(table, 3, rows_0_name));
    check_all("a repeated field keeps both", mt_eval(m, E("json-at", decode(twice), E("a", 0))), at(twice, 2, a_0));
    check_all("a missing field has none", mt_eval(m, E("json-at", decode("{}"), E("missing"))), at("{}", 1, missing));
    assert(answers_are(mt_eval(m, E("json-at", 42, mt_unit())), E(42)) && "an empty path is the input");
    cJSON *empty = cJSON_CreateObject();
    char *braces = cJSON_PrintUnformatted(empty);
    assert(answers_are(mt_eval(m, E("json-encode", E("dict-space", mt_unit()))), E(T(braces)))
           && "an empty object encodes as cJSON prints one");
    cJSON_free(braces);
    cJSON_Delete(empty);
    assert(answers_are(mt_eval(m, E("let", V("d"), E("dict-space", E(E("from", 1), E("internal", 2))),
                                    E("collapse", E("get-keys", V("d"))))), E(E("from", "internal")))
           && "keys come back in the order given");

    /* The native writer's layouts, which cJSON reads as the same list. */
    assert(answers_are(mt_eval(m, E("json-pretty", E(1, 2))), E(T("[1, 2 ]"))) && "the default layout");
    assert(answers_are(mt_eval(m, E("json-pretty", E(1, 2), 1)), E(T("[\n  1,\n  2\n]"))) && "width one expands it");
    assert(atom_is(read_json("[1, 2 ]"), E(1, 2)) && "cJSON reads the one layout");
    assert(atom_is(read_json("[\n  1,\n  2\n]"), E(1, 2)) && "and the other");
    char *flat = engine_text(m, E("json-pretty", E(1, 2), 0));
    assert(answers_are(mt_eval(m, decode(flat)), E(read_json(flat))) && "width zero keeps one line");
    free(flat);

    /* JSON Lines. */
    const char *three = "1\r\ntrue\nnull\n";
    check_all("each line a value", mt_eval(m, E("json-lines-decode", T(three))), lines(three));
    char out[8 * MOST] = "";
    record(out, sizeof out, cJSON_CreateNumber(1));
    record(out, sizeof out, cJSON_CreateTrue());
    record(out, sizeof out, cJSON_CreateString("é"));
    assert(answers_are(mt_eval(m, E("json-lines-encode", E(1, B(true), T("é")))), E(T(out))) && "each record ends with LF");
    assert(!mt_first(mt_eval(m, E("json-lines-decode", T("")))) && mt_ok() && "empty input has no records");
    assert(answers_are(mt_eval(m, E("json-lines-encode", mt_unit())), E(T(""))) && "and no records encode as nothing");

    /* Files: the engine writes, C reads the bytes, the engine reads back. */
    require("import lib_file", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_file")))));
    mt_atom *document = mt_one(mt_eval(m, E("temp-path!", T("json-document"))));
    require("a scratch path", document && mt_kind_of(document) == MT_TEXT);
    assert(answers_are(mt_eval(m, E("json-write!", mt_keep(document), E(7, 8))), E(B(true))) && "json-write!");
    char *disk = slurp(mt_name(document));
    assert(answers_are(mt_eval(m, E("json-read!", mt_keep(document))), E(read_json(disk)))
           && "json-read! answers what cJSON reads on disk");
    free(disk);
    assert(answers_are(mt_eval(m, E("delete-file!", document)), E(B(true))) && "the file goes");

    mt_atom *log = mt_one(mt_eval(m, E("temp-path!", T("json-lines"))));
    require("a scratch path", log && mt_kind_of(log) == MT_TEXT);
    assert(answers_are(mt_eval(m, E("json-lines-write!", mt_keep(log), E(1, B(true), T("é")))), E(B(true))) && "json-lines-write!");
    disk = slurp(mt_name(log));
    check_all("json-lines-read! answers each line cJSON reads", mt_eval(m, E("json-lines-read!", mt_keep(log))),
              lines(disk));
    free(disk);
    assert(answers_are(mt_eval(m, E("delete-file!", log)), E(B(true))) && "that file goes too");
    mt_close(m);
    return 0;
}
#else
#include <stdio.h>

/* Without cJSON's headers the program only says what it needs. */
int main(void)
{
    fputs("05-json_lib.c needs cJSON: install its development files, then build with\n"
          "cc 05-json_lib.c $(pkg-config --cflags --libs cmetta libcjson)\n", stderr);
    return 77;
}
#endif
