/* Purpose: lib_yaml, held against libyaml called from C, the library under
 *   SWI's yaml package that the engine's reader and writer use. C reads a
 *   document with libyaml's event parser, because only events tell an
 *   untagged plain scalar from a tagged or quoted one, and resolves each plain
 *   scalar by the YAML 1.2 core schema's own table, written as the POSIX
 *   regular expressions the specification states it in. It writes with
 *   libyaml's event emitter and the settings SWI's writer passes: implicit
 *   document markers, block collections, plain scalars, Unicode on. A mapping
 *   is (Mapping ((Key Value) ...)) in C, keys in the standard order as SWI's
 *   dicts keep them; the symbol cannot collide with a YAML string, which
 *   decodes as text. C refuses what the library refuses: a second document,
 *   a tag with no standard meaning, a repeated key, malformed text, and a
 *   mapping atom that is no pair.
 * Assumes: libyaml, found through pkg-config as yaml-0.1.
 * Guarantees: all thirty-two claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include <inttypes.h>
#include <math.h>
#include <regex.h>
#include <yaml.h>

enum { MOST = 16, TEXT = 1024 };

/* The YAML 1.2.2 core schema's tag resolution, section 10.3.2 [source:
   https://yaml.org/spec/1.2.2/#1032-tag-resolution]. The empty plain scalar
   is the one departure: the core schema reads it as null, and SWI's reader,
   which the library keeps, as the empty string [source:
   lib/lib_yaml/lib_yaml.pl, Fails when; commit=33c2d50c84b24c1a2c906225600e2a4ffdb662f1]. */
typedef enum resolved { AS_TEXT, AS_NULL, AS_BOOL, AS_INT, AS_OCTAL, AS_HEX, AS_FLOAT, AS_SPECIAL } resolved;

static resolved resolve(const char *plain)
{
    static const struct {
        const char *pattern;
        resolved as;
    } table[] = {
        { "^(null|Null|NULL|~)$", AS_NULL },
        { "^(true|True|TRUE|false|False|FALSE)$", AS_BOOL },
        { "^[-+]?[0-9]+$", AS_INT },
        { "^0o[0-7]+$", AS_OCTAL },
        { "^0x[0-9a-fA-F]+$", AS_HEX },
        { "^[-+]?(\\.[0-9]+|[0-9]+(\\.[0-9]*)?)([eE][-+]?[0-9]+)?$", AS_FLOAT },
        { "^([-+]?\\.(inf|Inf|INF)|\\.(nan|NaN|NAN))$", AS_SPECIAL },
    };
    for (size_t i = 0; i < sizeof table / sizeof *table; i++) {
        regex_t re;
        require("the schema's pattern compiles", regcomp(&re, table[i].pattern, REG_EXTENDED | REG_NOSUB) == 0);
        bool match = regexec(&re, plain, 0, NULL, 0) == 0;
        regfree(&re);
        if (match) return table[i].as;
    }
    return AS_TEXT;
}

static mt_atom *scalar_of(const char *text, bool plain)
{
    if (!plain) return T(text);
    switch (resolve(text)) {
    case AS_NULL: return S("Null");
    case AS_BOOL: return B(text[0] == 't' || text[0] == 'T');
    case AS_INT: return mt_num(strtoll(text, NULL, 10));
    case AS_OCTAL: return mt_num(strtoll(text + 2, NULL, 8));
    case AS_HEX: return mt_num(strtoll(text + 2, NULL, 16));
    case AS_FLOAT: return mt_real(strtod(text, NULL));
    case AS_SPECIAL: return mt_real(strpbrk(text, "nN") ? NAN : text[0] == '-' ? -HUGE_VAL : HUGE_VAL);
    case AS_TEXT: break;
    }
    return T(text);
}

/* The standard tags a node may carry; anything else is refused. */
static bool standard_tag(const char *tag)
{
    static const char *const known[] = { "str", "int", "float", "bool", "null", "binary", "seq", "map" };
    if (!tag || strcmp(tag, "!") == 0) return true;
    for (size_t i = 0; i < sizeof known / sizeof *known; i++) {
        char full[64];
        snprintf(full, sizeof full, "tag:yaml.org,2002:%s", known[i]);
        if (strcmp(tag, full) == 0) return true;
    }
    return false;
}

static bool is_mapping(const mt_atom *v)
{
    return mt_kind_of(v) == MT_EXPR && mt_len(v) == 2 && mt_kind_of(mt_at(v, 0)) == MT_SYMBOL && strcmp(mt_name(mt_at(v, 0)), "Mapping") == 0;
}

static int by_key(const void *a, const void *b)
{
    return mt_compare(mt_at(*(mt_atom *const *)a, 0), mt_at(*(mt_atom *const *)b, 0));
}

/* One node from its first event; NULL on a refusal. */
static mt_atom *node_from(yaml_parser_t *p, yaml_event_t *ev);

static mt_atom *collection(yaml_parser_t *p, bool mapping)
{
    mt_atom *kids[MOST];
    size_t n = 0;
    for (;;) {
        yaml_event_t ev;
        if (!yaml_parser_parse(p, &ev)) break;
        if (ev.type == YAML_SEQUENCE_END_EVENT || ev.type == YAML_MAPPING_END_EVENT) {
            yaml_event_delete(&ev);
            if (!mapping) return mt_exprv(n, kids);
            qsort(kids, n, sizeof *kids, by_key);
            for (size_t i = 1; i < n; i++)
                if (by_key(&kids[i - 1], &kids[i]) == 0) goto refused;
            return E("Mapping", mt_exprv(n, kids));
        }
        mt_atom *first = node_from(p, &ev);
        if (!first) goto refused;
        if (!mapping) {
            require("room for the items", n < MOST);
            kids[n++] = first;
            continue;
        }
        yaml_event_t next;
        mt_atom *value = yaml_parser_parse(p, &next) ? node_from(p, &next) : NULL;
        if (!value) {
            mt_drop(first);
            goto refused;
        }
        mt_atom *key = mt_kind_of(first) == MT_TEXT ? S(mt_name(first)) : mt_keep(first);
        mt_drop(first);
        require("room for the pairs", n < MOST);
        kids[n++] = E(key, value);
    }
refused:
    while (n) mt_drop(kids[--n]);
    return NULL;
}

static mt_atom *node_from(yaml_parser_t *p, yaml_event_t *ev)
{
    mt_atom *out = NULL;
    switch (ev->type) {
    case YAML_SCALAR_EVENT:
        if (standard_tag((const char *)ev->data.scalar.tag))
            out = scalar_of((const char *)ev->data.scalar.value,
                            ev->data.scalar.style == YAML_PLAIN_SCALAR_STYLE &&
                                (!ev->data.scalar.tag || strcmp((const char *)ev->data.scalar.tag, "tag:yaml.org,2002:str") != 0));
        break;
    case YAML_SEQUENCE_START_EVENT:
        if (standard_tag((const char *)ev->data.sequence_start.tag)) out = collection(p, false);
        break;
    case YAML_MAPPING_START_EVENT:
        if (standard_tag((const char *)ev->data.mapping_start.tag)) out = collection(p, true);
        break;
    default:
        require("no aliases in these documents", ev->type != YAML_ALIAS_EVENT);
    }
    yaml_event_delete(ev);
    return out;
}

/* The type of the next event, consumed; YAML_NO_EVENT when libyaml stops. */
static yaml_event_type_t next_event(yaml_parser_t *p)
{
    yaml_event_t ev;
    if (!yaml_parser_parse(p, &ev)) return YAML_NO_EVENT;
    yaml_event_type_t type = ev.type;
    yaml_event_delete(&ev);
    return type;
}

/* A stream of one document; NULL when libyaml refuses the text or it holds
   a second document. An empty stream is Null. */
static mt_atom *decoded(const char *text)
{
    yaml_parser_t p;
    yaml_event_t ev;
    mt_atom *out = NULL;
    require("a parser", yaml_parser_initialize(&p));
    yaml_parser_set_input_string(&p, (const unsigned char *)text, strlen(text));
    yaml_event_type_t first = next_event(&p) == YAML_STREAM_START_EVENT ? next_event(&p) : YAML_NO_EVENT;
    if (first == YAML_STREAM_END_EVENT) out = S("Null");
    else if (first == YAML_DOCUMENT_START_EVENT) {
        out = yaml_parser_parse(&p, &ev) ? node_from(&p, &ev) : NULL;
        if (!out || next_event(&p) != YAML_DOCUMENT_END_EVENT || next_event(&p) != YAML_STREAM_END_EVENT) {
            mt_drop(out);
            out = NULL;
        }
    }
    yaml_parser_delete(&p);
    return out;
}

/* Writing: libyaml's events, as SWI's yaml_write/3 sends them. */
static bool emitted(yaml_emitter_t *e, yaml_event_t *ev, int ok) { return ok && yaml_emitter_emit(e, ev); }

static bool emit(yaml_emitter_t *e, const mt_atom *v)
{
    yaml_event_t ev;
    if (is_mapping(v)) {
        const mt_atom *pairs = mt_at(v, 1);
        bool ok = emitted(e, &ev, yaml_mapping_start_event_initialize(&ev, NULL, NULL, 0, YAML_BLOCK_MAPPING_STYLE));
        for (size_t i = 0; ok && i < mt_len(pairs); i++) ok = emit(e, mt_at(mt_at(pairs, i), 0)) && emit(e, mt_at(mt_at(pairs, i), 1));
        return ok && emitted(e, &ev, yaml_mapping_end_event_initialize(&ev));
    }
    if (mt_kind_of(v) == MT_EXPR) {
        bool ok = emitted(e, &ev, yaml_sequence_start_event_initialize(&ev, NULL, NULL, 0, YAML_BLOCK_SEQUENCE_STYLE));
        for (size_t i = 0; ok && i < mt_len(v); i++) ok = emit(e, mt_at(v, i));
        return ok && emitted(e, &ev, yaml_sequence_end_event_initialize(&ev));
    }
    char digits[32];
    const char *text, *tag = NULL;
    switch (mt_kind_of(v)) {
    case MT_INT:
        snprintf(digits, sizeof digits, "%" PRId64, mt_int(v));
        text = digits;
        break;
    case MT_BOOL:
        text = mt_truth(v) ? "true" : "false";
        break;
    case MT_SYMBOL:
        text = strcmp(mt_name(v), "Null") == 0 ? "null" : mt_name(v);
        if (strcmp(mt_name(v), "Null") != 0 && resolve(text) != AS_TEXT) tag = "tag:yaml.org,2002:str";
        break;
    case MT_TEXT:
        text = mt_name(v);
        if (resolve(text) != AS_TEXT) tag = "tag:yaml.org,2002:str";
        break;
    default:
        require("a scalar these documents hold", false);
        return false;
    }
    return emitted(e, &ev, yaml_scalar_event_initialize(&ev, NULL, (yaml_char_t *)tag, (yaml_char_t *)text, -1, tag == NULL, tag == NULL,
                                                        YAML_PLAIN_SCALAR_STYLE));
}

static mt_atom *encoded(const mt_atom *v)
{
    yaml_emitter_t e;
    yaml_event_t ev;
    unsigned char buf[TEXT];
    size_t written = 0;
    require("an emitter", yaml_emitter_initialize(&e));
    yaml_emitter_set_output_string(&e, buf, sizeof buf, &written);
    yaml_emitter_set_unicode(&e, 1);
    bool ok = emitted(&e, &ev, yaml_stream_start_event_initialize(&ev, YAML_UTF8_ENCODING)) &&
              emitted(&e, &ev, yaml_document_start_event_initialize(&ev, NULL, NULL, NULL, 1)) && emit(&e, v) &&
              emitted(&e, &ev, yaml_document_end_event_initialize(&ev, 1)) && emitted(&e, &ev, yaml_stream_end_event_initialize(&ev));
    yaml_emitter_delete(&e);
    require("libyaml writes the document", ok);
    return mt_textn((const char *)buf, written);
}

/* A mapping from atoms that must each be (Key Value); NULL when one is not. */
static mt_atom *mapping_of(const mt_atom *atoms)
{
    mt_atom *pairs[MOST];
    size_t n = mt_len(atoms);
    for (size_t i = 0; i < n; i++)
        if (mt_kind_of(mt_at(atoms, i)) != MT_EXPR || mt_len(mt_at(atoms, i)) != 2) return NULL;
    for (size_t i = 0; i < n; i++) pairs[i] = mt_keep(mt_at(atoms, i));
    qsort(pairs, n, sizeof *pairs, by_key);
    return E("Mapping", mt_exprv(n, pairs));
}

/* Following a path of keys and indices through C's value; NULL where it
   leads nowhere. */
static const mt_atom *at_path(const mt_atom *v, const mt_atom *path)
{
    for (size_t i = 0; v && i < mt_len(path); i++) {
        const mt_atom *step = mt_at(path, i);
        if (is_mapping(v)) {
            const mt_atom *pairs = mt_at(v, 1), *found = NULL;
            for (size_t k = 0; k < mt_len(pairs) && !found; k++)
                if (mt_compare(mt_at(mt_at(pairs, k), 0), step) == 0) found = mt_at(mt_at(pairs, k), 1);
            v = found;
        } else
            v = mt_kind_of(v) == MT_EXPR && mt_kind_of(step) == MT_INT && (size_t)mt_int(step) < mt_len(v) ? mt_at(v, (size_t)mt_int(step)) : NULL;
    }
    return v;
}

static mt_atom *keys_of(const mt_atom *mapping)
{
    const mt_atom *pairs = mt_at(mapping, 1);
    mt_atom *keys[MOST];
    for (size_t i = 0; i < mt_len(pairs); i++) keys[i] = mt_keep(mt_at(mt_at(pairs, i), 0));
    return mt_exprv(mt_len(pairs), keys);
}

static mt_atom *verdict(bool holds) { return S(holds ? "fine" : "refused"); }
static mt_atom *guarded(mt_atom *goal) { return E("if-error", E("catch", goal), "refused", "fine"); }

static bool computed(mt_atom *value)
{
    mt_drop(value);
    return value != NULL;
}

static void check_found(const char *claim, mt_answers *got, const mt_atom *found)
{
    if (found) check_value(claim, got, mt_keep(found));
    else check_none(claim, got);
}

int main(void)
{
    metta *m = open_engine();
    require("import lib_yaml", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_yaml")))));

    /* A mapping decodes as a space of pairs, queried as lib_json's are. */
    const char *text = "name: petta\nversion: 1.5\ntags:\n  - prolog\n  - metta\nlimits:\n  depth: 3\n  strict: true\n  note:\n  nothing: ~\n";
    mt_atom *c_conf = decoded(text), *conf = mt_one(mt_eval(m, E("yaml-decode", T(text))));
    require("both read the document", c_conf && conf);
    check_answers("the keys", mt_eval(m, E("collapse", E("get-keys", mt_keep(conf)))), keys_of(c_conf));
    static const struct {
        const char *claim;
        bool getter;
        const char *steps[2];
        int64_t index;
    } queries[] = {
        { "a string", true, { "name" }, -1 },           { "a float", true, { "version" }, -1 },
        { "a sequence", true, { "tags" }, -1 },         { "a path through a sequence", false, { "tags" }, 0 },
        { "a nested integer", false, { "limits", "depth" }, -1 }, { "a boolean", false, { "limits", "strict" }, -1 },
        { "a key with no value is the empty string", false, { "limits", "note" }, -1 },
        { "~ is Null", false, { "limits", "nothing" }, -1 },
    };
    for (size_t i = 0; i < sizeof queries / sizeof *queries; i++) {
        mt_atom *steps[3];
        size_t n = 0;
        for (size_t k = 0; k < 2 && queries[i].steps[k]; k++) steps[n++] = S(queries[i].steps[k]);
        if (queries[i].index >= 0) steps[n++] = mt_num(queries[i].index);
        mt_atom *path = mt_exprv(n, steps);
        mt_atom *goal = queries[i].getter ? E("get-value", mt_keep(conf), mt_keep(mt_at(path, 0))) : E("json-at", mt_keep(conf), mt_keep(path));
        check_found(queries[i].claim, mt_eval(m, goal), at_path(c_conf, path));
        mt_drop(path);
    }
    mt_atom *missing = E("missing");
    check_answers("an absent key has no answer", mt_eval(m, E("collapse", E("get-value", mt_keep(conf), "missing"))),
                  at_path(c_conf, missing) ? E(mt_keep(at_path(c_conf, missing))) : mt_unit());
    mt_drop(missing);

    /* Documents that are no mapping. */
    static const char *const scalars[][2] = {
        { "a sequence", "- 1\n- 2\n" }, { "a string", "just text\n" }, { "an integer", "42\n" },
        { "a boolean", "true\n" },      { "null", "null\n" },          { "an empty document is Null", "" },
    };
    for (size_t i = 0; i < sizeof scalars / sizeof *scalars; i++)
        check_answers(scalars[i][0], mt_eval(m, E("yaml-decode", T(scalars[i][1]))), decoded(scalars[i][1]));
    mt_atom *no = decoded("k: no\n"), *k_path = E("k");
    check_answers("no is a string in the core schema", mt_eval(m, E("json-at", E("yaml-decode", T("k: no\n")), mt_keep(k_path))),
                  mt_keep(at_path(no, k_path)));

    /* Writing. */
    mt_atom *small = decoded("a: 1\nb:\n  - x\n"), *items = E(1, 2, "Null", B(true)), *word = T("text"), *seven = mt_num(7), *null = S("Null");
    check_answers("a round trip", mt_eval(m, E("yaml-encode", E("yaml-decode", T("a: 1\nb:\n  - x\n")))), encoded(small));
    check_answers("a sequence of scalars", mt_eval(m, E("yaml-encode", mt_keep(items))), encoded(items));
    check_answers("a string", mt_eval(m, E("yaml-encode", mt_keep(word))), encoded(word));
    check_answers("a number", mt_eval(m, E("yaml-encode", mt_keep(seven))), encoded(seven));
    check_answers("Null", mt_eval(m, E("yaml-encode", mt_keep(null))), encoded(null));
    mt_atom *ab_pairs = E(E("a", 1), E("b", T("two"))), *ab = mapping_of(ab_pairs), *k_pairs = E(E("k", 1)), *k_map = mapping_of(k_pairs);
    check_answers("a space of pairs", mt_eval(m, E("yaml-encode", E("dict-space", mt_keep(ab_pairs)))), encoded(ab));
    mt_atom *k_text = encoded(k_map), *k_back = decoded(mt_name(k_text));
    check_answers("which reads back", mt_eval(m, E("json-at", E("yaml-decode", E("yaml-encode", E("dict-space", mt_keep(k_pairs)))), mt_keep(k_path))),
                  mt_keep(at_path(k_back, k_path)));

    /* The file doors, inside lib_file's scope. */
    require("write-and-read",
            mt_add(m, E("=", E("write-and-read", V("dir")),
                        E("let", V("path"), E("path-join", V("dir"), T("conf.yaml")),
                          E("let", V("written"), E("yaml-write!", V("path"), E("dict-space", E(E("host", T("localhost")), E("port", 8080)))),
                            E("let", V("text"), E("read-file!", V("path")),
                              E(V("text"), E("json-at", E("yaml-read!", V("path")), E("port")))))))));
    mt_atom *server_pairs = E(E("host", T("localhost")), E("port", 8080)), *server = mapping_of(server_pairs), *server_text = encoded(server),
            *server_back = decoded(mt_name(server_text)), *port = E("port");
    check_answers("write then read", mt_eval(m, E("with-temp-dir", T("yaml-lib"), "write-and-read")),
                  E(mt_keep(server_text), mt_keep(at_path(server_back, port))));

    /* The refusals. */
    static const char *const refused_texts[][2] = {
        { "a second document", "a: 1\n---\nb: 2\n" },
        { "a tag with no standard meaning", "k: !foo 1\n" },
        { "malformed text", "a: [1,\n" },
        { "a repeated key", "dup: 1\ndup: 2\n" },
    };
    for (size_t i = 0; i < sizeof refused_texts / sizeof *refused_texts; i++)
        check_answers(refused_texts[i][0], mt_eval(m, guarded(E("yaml-decode", T(refused_texts[i][1])))),
                      verdict(computed(decoded(refused_texts[i][1]))));
    check_answers("a number is no text", mt_eval(m, guarded(E("yaml-decode", mt_keep(seven)))), verdict(mt_kind_of(seven) == MT_TEXT));
    mt_atom *notamap = mt_one(mt_eval(m, E("new-space"))), *triple = E(E("a", 1, 2));
    require("a new space", notamap && mt_one_truth(mt_eval(m, E("add-atom", mt_keep(notamap), mt_keep(mt_at(triple, 0))))));
    check_answers("a space that is no mapping", mt_eval(m, guarded(E("yaml-encode", mt_keep(notamap)))), verdict(computed(mapping_of(triple))));
    mt_atom *symbols = E("one", "two");
    check_answers("symbols write as the strings they spell", mt_eval(m, E("yaml-encode", mt_keep(symbols))), encoded(symbols));

    mt_atom *held[] = { c_conf, conf, no, k_path, small, items, word, seven, null, ab_pairs, ab, k_pairs, k_map, k_text, k_back,
                        server_pairs, server, server_text, server_back, port, notamap, triple, symbols };
    for (size_t i = 0; i < sizeof held / sizeof *held; i++) mt_drop(held[i]);
    return done(m);
}
