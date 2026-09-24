/* Purpose: lib_json, held against cJSON, a JSON parser C programs link.
 *   C parses the text the engine decodes and reads its answer off cJSON's tree:
 *   an array is an expression, a scalar itself, an object's keys and values
 *   are what the decoded space holds, and a path follows every field of a
 *   repeated name. What the engine encodes C parses back, a JSON Lines record
 *   is a value cJSON prints ended by LF, and a file the engine writes C reads
 *   from disk and parses. The two pretty layouts are the native writer's own
 *   and stay literal, each read by cJSON as the same list.
 * Assumes: libcjson, found through pkg-config.
 * Guarantees: all twenty-eight claims of the original hold [tested: make
 *   twins; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include <cjson/cJSON.h>

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
    check_answers_(claim, got, want.n, want.item);
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
    metta *m = open_engine();
    require("import lib_json", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_json")))));

    /* An object decodes to a space whose atoms are its fields. */
    const char *pair = "{\"a\":1,\"b\":2}";
    check_all("get-keys answers the object's keys", mt_eval(m, E("let", V("d"), decode(pair), E("get-keys", V("d")))),
              keys(pair));
    const char *const a[] = { "a" }, *const missing[] = { "missing" };
    check_all("get-value answers a field", mt_eval(m, E("let", V("d"), decode(pair), E("get-value", V("d"), "a"))),
              at(pair, 1, a));
    check_none("an absent key answers nothing",
               mt_eval(m, E("let", V("d"), decode("{\"a\":1}"), E("get-value", V("d"), "missing"))));

    const char *const scalars[] = { "[1,2,3]", "\"plain\"", "42", "true", "null" };
    for (size_t i = 0; i < sizeof scalars / sizeof *scalars; i++)
        check_answers(scalars[i], mt_eval(m, decode(scalars[i])), read_json(scalars[i]));

    const char *nested = "{\"c\":{\"d\":2}}";
    const char *const c_d[] = { "c", "d" };
    check_all("an inner object is a space too",
              mt_eval(m, E("let", V("outer"), decode(nested),
                           E("let", V("inner"), E("get-value", V("outer"), "c"), E("get-value", V("inner"), "d")))),
              at(nested, 2, c_d));

    /* Encoding inverts decoding, and cJSON reads the engine's text as the
       value it encoded. */
    char *list = engine_text(m, E("json-encode", E(1, 2, 3)));
    check_atom("cJSON reads the encoded list as the list", read_json(list), E(1, 2, 3));
    check_answers("encoding inverts decoding", mt_eval(m, decode(list)), read_json(list));
    char *text = engine_text(m, E("json-encode", T("text")));
    check_atom("and a text", read_json(text), T("text"));
    check_answers("for text too", mt_eval(m, decode(text)), read_json(text));
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
    check_answers("dict-space builds one from pairs",
                  mt_eval(m, E("let", V("d"), E("dict-space", mt_exprv(2, rows)), E("get-value", V("d"), "name"))), found);

    /* Paths mix field names with zero-based indexes. */
    const char *table = "{\"rows\":[{\"name\":\"ann\"}]}", *twice = "{\"a\":[1],\"a\":[2]}";
    const char *const rows_0_name[] = { "rows", "0", "name" }, *const a_0[] = { "a", "0" };
    check_all("a path through an array", mt_eval(m, E("json-at", decode(table), E("rows", 0, "name"))),
              at(table, 3, rows_0_name));
    check_all("a repeated field keeps both", mt_eval(m, E("json-at", decode(twice), E("a", 0))), at(twice, 2, a_0));
    check_all("a missing field has none", mt_eval(m, E("json-at", decode("{}"), E("missing"))), at("{}", 1, missing));
    check_answers("an empty path is the input", mt_eval(m, E("json-at", 42, mt_unit())), 42);
    cJSON *empty = cJSON_CreateObject();
    char *braces = cJSON_PrintUnformatted(empty);
    check_answers("an empty object encodes as cJSON prints one", mt_eval(m, E("json-encode", E("dict-space", mt_unit()))),
                  T(braces));
    cJSON_free(braces);
    cJSON_Delete(empty);
    check_answers("keys come back in the order given",
                  mt_eval(m, E("let", V("d"), E("dict-space", E(E("from", 1), E("internal", 2))),
                               E("collapse", E("get-keys", V("d"))))),
                  E("from", "internal"));

    /* The native writer's layouts, which cJSON reads as the same list. */
    check_answers("the default layout", mt_eval(m, E("json-pretty", E(1, 2))), T("[1, 2 ]"));
    check_answers("width one expands it", mt_eval(m, E("json-pretty", E(1, 2), 1)), T("[\n  1,\n  2\n]"));
    check_atom("cJSON reads the one layout", read_json("[1, 2 ]"), E(1, 2));
    check_atom("and the other", read_json("[\n  1,\n  2\n]"), E(1, 2));
    char *flat = engine_text(m, E("json-pretty", E(1, 2), 0));
    check_answers("width zero keeps one line", mt_eval(m, decode(flat)), read_json(flat));
    free(flat);

    /* JSON Lines. */
    const char *three = "1\r\ntrue\nnull\n";
    check_all("each line a value", mt_eval(m, E("json-lines-decode", T(three))), lines(three));
    char out[8 * MOST] = "";
    record(out, sizeof out, cJSON_CreateNumber(1));
    record(out, sizeof out, cJSON_CreateTrue());
    record(out, sizeof out, cJSON_CreateString("é"));
    check_answers("each record ends with LF", mt_eval(m, E("json-lines-encode", E(1, B(true), T("é")))), T(out));
    check_none("empty input has no records", mt_eval(m, E("json-lines-decode", T(""))));
    check_answers("and no records encode as nothing", mt_eval(m, E("json-lines-encode", mt_unit())), T(""));

    /* Files: the engine writes, C reads the bytes, the engine reads back. */
    require("import lib_file", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_file")))));
    mt_atom *document = mt_one(mt_eval(m, E("temp-path!", T("json-document"))));
    require("a scratch path", document && mt_kind_of(document) == MT_TEXT);
    check_answers("json-write!", mt_eval(m, E("json-write!", mt_keep(document), E(7, 8))), B(true));
    char *disk = slurp(mt_name(document));
    check_answers("json-read! answers what cJSON reads on disk", mt_eval(m, E("json-read!", mt_keep(document))),
                  read_json(disk));
    free(disk);
    check_answers("the file goes", mt_eval(m, E("delete-file!", document)), B(true));

    mt_atom *log = mt_one(mt_eval(m, E("temp-path!", T("json-lines"))));
    require("a scratch path", log && mt_kind_of(log) == MT_TEXT);
    check_answers("json-lines-write!", mt_eval(m, E("json-lines-write!", mt_keep(log), E(1, B(true), T("é")))), B(true));
    disk = slurp(mt_name(log));
    check_all("json-lines-read! answers each line cJSON reads", mt_eval(m, E("json-lines-read!", mt_keep(log))),
              lines(disk));
    free(disk);
    check_answers("that file goes too", mt_eval(m, E("delete-file!", log)), B(true));
    return done(m);
}
