/* Purpose: lib_http, held against libcurl, the C world's HTTP client. C
 *   defines the routes as MeTTa equations and the engine serves them; every
 *   request the engine's client makes, C makes too through libcurl, and the
 *   engine's status, fields and bytes are held against what a C client
 *   read: field names matched case-insensitively, a repeated field in the
 *   order it arrived, and Content-Length as the decimal count RFC 9110
 *   defines. One C description of a request builds the engine's options,
 *   drives libcurl and decides every refusal, the rules RFC 9110 gives
 *   methods, field names and values, with the scheme read by libcurl's own
 *   URL parser. The streaming door is held against C reading the same bytes
 *   in two parts, and the two lifecycle refusals run inside cmetta's own
 *   transaction door.
 * Build: cc 38-http_lib.c $(pkg-config --cflags --libs cmetta libcurl)
 * Guarantees: all forty-one claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define _POSIX_C_SOURCE 200809L
#define MT_SHORTHAND
#if __has_include(<curl/curl.h>)
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <curl/curl.h>
#include <math.h>
#include <strings.h>

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

/* The methods the library's client names, beside libcurl's spelling of
   each, which is the method token upper-cased [source: RFC 9110, section
   9]. */
static const char *const methods[] = { "delete", "get", "head", "post", "put", "patch", "options" };
enum { METHODS = sizeof methods / sizeof *methods };

static bool known_method(const char *name)
{
    for (size_t i = 0; i < METHODS; i++)
        if (strcmp(methods[i], name) == 0) return true;
    return false;
}

/* One option of a request, as C describes it. A body keeps its bytes as
   ints so a value past a byte can be described, and refused. */
typedef struct option {
    enum { HEADER, BODY, TIMEOUT, REDIRECT } kind;
    const char *name, *value;
    const char *media;
    const int *bytes;
    size_t n_bytes;
    double seconds;
    bool follow;
} option;

typedef struct request {
    const char *method, *url;
    const option *options;
    size_t n_options;
} request;

/* The engine's spelling of the same options. */
static mt_atom *options_atom(const request *r)
{
    mt_atom **kids = malloc((r->n_options + 1) * sizeof *kids);
    require("room for the options", kids != NULL);
    for (size_t i = 0; i < r->n_options; i++) {
        const option *o = &r->options[i];
        kids[i] = o->kind == HEADER    ? E("header", T(o->name), T(o->value))
                  : o->kind == BODY    ? E("body", T(o->media), mt_array(o->n_bytes, o->bytes))
                  : o->kind == TIMEOUT ? E("timeout", o->seconds)
                                       : E("redirect", B(o->follow));
    }
    mt_atom *out = mt_exprv(r->n_options, kids);
    free(kids);
    return out;
}

static mt_atom *engine_request(const char *door, const request *r) { return E(door, S(r->method), T(r->url), options_atom(r)); }

/* RFC 9110's token characters, section 5.6.2. */
static bool token(const char *text)
{
    if (!*text) return false;
    for (const char *p = text; *p; p++)
        if (!isalnum((unsigned char)*p) && !strchr("!#$%&'*+-.^_`|~", *p)) return false;
    return true;
}

/* A field value's characters: HTAB, visible ASCII, space and obs-text
   [source: RFC 9110, section 5.5]. */
static bool field_text(const char *text)
{
    for (const unsigned char *p = (const unsigned char *)text; *p; p++)
        if (*p != '\t' && (*p < 32 || *p == 127)) return false;
    return true;
}

/* Fields the transport writes itself, which a request may not set. */
static bool client_owned(const char *name)
{
    static const char *const owned[] = { "host", "content-type", "content-length", "transfer-encoding", "connection" };
    for (size_t i = 0; i < sizeof owned / sizeof *owned; i++)
        if (strcasecmp(owned[i], name) == 0) return true;
    return false;
}

/* type "/" subtype, then parameters [source: RFC 9110, section 8.3.1]. */
static bool media_type(const char *text)
{
    if (!field_text(text)) return false;
    size_t n = strcspn(text, ";");
    char *type = strndup(text, n);
    require("room for the media type", type != NULL);
    while (n && type[n - 1] == ' ') type[--n] = '\0';
    char *slash = strchr(type, '/');
    bool fine = slash != NULL;
    if (fine) {
        *slash = '\0';
        fine = token(type) && token(slash + 1);
    }
    free(type);
    return fine;
}

/* An http or https URL with a host, as libcurl's own parser reads it, in
   visible ASCII. */
static bool http_url(const char *url)
{
    for (const unsigned char *p = (const unsigned char *)url; *p; p++)
        if (*p <= 32 || *p == 127) return false;
    CURLU *u = curl_url();
    char *scheme = NULL, *host = NULL;
    bool fine = u && curl_url_set(u, CURLUPART_URL, url, CURLU_NON_SUPPORT_SCHEME) == CURLUE_OK &&
                curl_url_get(u, CURLUPART_SCHEME, &scheme, 0) == CURLUE_OK &&
                curl_url_get(u, CURLUPART_HOST, &host, 0) == CURLUE_OK && *host &&
                (strcmp(scheme, "http") == 0 || strcmp(scheme, "https") == 0);
    curl_free(scheme);
    curl_free(host);
    curl_url_cleanup(u);
    return fine;
}

/* Every rule a request must meet before any connection opens: a known
   method, an http URL, fields the client may set spelled as RFC 9110 spells
   them, bytes that are bytes, a positive timeout, and no option but a header
   given twice. */
static bool acceptable(const request *r)
{
    int seen[4] = { 0 };
    if (!known_method(r->method) || !http_url(r->url)) return false;
    for (size_t i = 0; i < r->n_options; i++) {
        const option *o = &r->options[i];
        if (o->kind != HEADER && seen[o->kind]++) return false;
        if (o->kind == HEADER && (!token(o->name) || client_owned(o->name) || !field_text(o->value))) return false;
        if (o->kind == BODY) {
            if (!media_type(o->media)) return false;
            for (size_t k = 0; k < o->n_bytes; k++)
                if (o->bytes[k] < 0 || o->bytes[k] > 255) return false;
        }
        if (o->kind == TIMEOUT && !(o->seconds > 0 && isfinite(o->seconds))) return false;
    }
    return true;
}

/* What a C client read: the status, the fields of the final response in
   the order they arrived, and the bytes. */
typedef struct exchange {
    long status;
    unsigned char *bytes;
    size_t n, cap;
    char **names, **values;
    size_t n_fields;
} exchange;

static void forget_fields(exchange *x)
{
    for (size_t i = 0; i < x->n_fields; i++) free(x->names[i]), free(x->values[i]);
    x->n_fields = 0;
}

static void release(exchange *x)
{
    forget_fields(x);
    free(x->names);
    free(x->values);
    free(x->bytes);
    *x = (exchange){ 0 };
}

static size_t on_body(char *data, size_t size, size_t count, void *user)
{
    exchange *x = user;
    size_t n = size * count;
    if (x->n + n > x->cap) {
        x->cap = 2 * (x->n + n);
        x->bytes = realloc(x->bytes, x->cap);
        require("room for the body", x->bytes != NULL);
    }
    memcpy(x->bytes + x->n, data, n);
    x->n += n;
    return n;
}

/* One header line: a status line starts a response over, which is how a
   followed redirect leaves only the last response's fields; a field is its
   name and its value without the surrounding whitespace. */
static size_t on_header(char *line, size_t size, size_t count, void *user)
{
    exchange *x = user;
    size_t n = size * count;
    const char *colon = memchr(line, ':', n);
    if (n >= 5 && memcmp(line, "HTTP/", 5) == 0) forget_fields(x);
    else if (colon) {
        const char *start = colon + 1, *end = line + n;
        while (start < end && (*start == ' ' || *start == '\t')) start++;
        while (end > start && strchr(" \t\r\n", end[-1])) end--;
        x->names = realloc(x->names, (x->n_fields + 1) * sizeof *x->names);
        x->values = realloc(x->values, (x->n_fields + 1) * sizeof *x->values);
        require("room for the fields", x->names && x->values);
        x->names[x->n_fields] = strndup(line, (size_t)(colon - line));
        x->values[x->n_fields++] = strndup(start, (size_t)(end - start));
    }
    return n;
}

/* "Name: value", as libcurl takes a field. */
static char *field_line(const char *name, const char *value)
{
    size_t n = strlen(name) + strlen(value) + 3;
    char *line = malloc(n);
    require("room for the field", line != NULL);
    snprintf(line, n, "%s: %s", name, value);
    return line;
}

/* The request made through libcurl, with libcurl's default fields; false
   when it could not be made. Its wildcard Accept field reaches the engine's
   handler as "*" since lib a5bd767, where the server had answered it 500. */
static bool fetch(const request *r, exchange *x)
{
    *x = (exchange){ 0 };
    CURL *curl = curl_easy_init();
    struct curl_slist *fields = NULL;
    char method[16], *line;
    unsigned char *body = NULL;
    size_t i;
    require("a libcurl handle", curl != NULL);
    for (i = 0; r->method[i] && i + 1 < sizeof method; i++) method[i] = (char)toupper((unsigned char)r->method[i]);
    method[i] = '\0';
    curl_easy_setopt(curl, CURLOPT_URL, r->url);
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "http,https");
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    if (strcmp(r->method, "head") == 0) curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    else curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method);
    for (i = 0; i < r->n_options; i++) {
        const option *o = &r->options[i];
        if (o->kind == HEADER || o->kind == BODY) {
            line = o->kind == HEADER ? field_line(o->name, o->value) : field_line("Content-Type", o->media);
            fields = curl_slist_append(fields, line);
            free(line);
        }
        if (o->kind == BODY) {
            body = malloc(o->n_bytes + 1);
            require("room for the request body", body != NULL);
            for (size_t k = 0; k < o->n_bytes; k++) body[k] = (unsigned char)o->bytes[k];
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)o->n_bytes);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
        } else if (o->kind == TIMEOUT)
            curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, (long)(o->seconds * 1000));
        else if (o->kind == REDIRECT) {
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, o->follow ? 1L : 0L);
            curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);
        }
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, fields);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, on_body);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, x);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, on_header);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, x);
    bool made = curl_easy_perform(curl) == CURLE_OK;
    if (made) curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &x->status);
    curl_slist_free_all(fields);
    curl_easy_cleanup(curl);
    free(body);
    return made;
}

static exchange fetched(const request *r)
{
    exchange x;
    require("the C request went through", fetch(r, &x));
    return x;
}

/* The values of a field, in the order they arrived: Content-Length as the
   decimal count RFC 9110 section 8.6 defines, any other its text. */
static size_t field_values(const exchange *x, const char *name, mt_atom ***out)
{
    size_t found = 0;
    *out = malloc((x->n_fields + 1) * sizeof **out);
    require("room for the values", *out != NULL);
    for (size_t i = 0; i < x->n_fields; i++)
        if (strcasecmp(x->names[i], name) == 0)
            (*out)[found++] = strcasecmp(name, "content-length") == 0 ? mt_num(strtoll(x->values[i], NULL, 10)) : T(x->values[i]);
    return found;
}

/* The field's one value. */
static mt_atom *field_value(const exchange *x, const char *name)
{
    mt_atom **values;
    size_t n = field_values(x, name, &values);
    require("the field arrived once", n == 1);
    mt_atom *value = values[0];
    free(values);
    return value;
}

/* A claim over every answer of the engine's http-header door. */
static void check_field(metta *m, const char *claim, const mt_atom *fields, const char *name, const exchange *x)
{
    mt_atom **values;
    size_t n = field_values(x, name, &values);
    assert(answers_are(mt_eval(m, E("http-header", mt_keep(fields), T(name))), mt_exprv(n, values)) && claim);
    free(values);
}

/* Bytes the engine answered, as the C text they spell. */
static char *text_of(const mt_atom *bytes)
{
    char *text = malloc(mt_len(bytes) + 1);
    require("room for the text", text != NULL);
    for (size_t i = 0; i < mt_len(bytes); i++) text[i] = (char)mt_int(mt_at(bytes, i));
    text[mt_len(bytes)] = '\0';
    return text;
}

/* A body read in parts: what a stream hands over, and a close that
   releases once, however often it is asked. */
typedef struct reader {
    const unsigned char *bytes;
    size_t n, at;
    bool open;
} reader;

static mt_atom *read_part(reader *r, size_t most)
{
    size_t take = r->n - r->at < most ? r->n - r->at : most;
    mt_atom *out = mt_array(take, r->bytes + r->at);
    r->at += take;
    return out;
}

static bool close_reader(reader *r)
{
    r->open = false;
    return true;
}

static mt_atom *verdict(bool holds) { return S(holds ? "fine" : "refused"); }
static mt_atom *guarded(mt_atom *goal) { return E("if-error", E("catch", goal), "refused", "fine"); }

static mt_atom *value_of(metta *m, mt_atom *goal)
{
    mt_atom *v = mt_one(mt_eval(m, goal));
    require("a value", v != NULL);
    return v;
}

/* A server needs a worker, and its lifecycle may not change inside a
   transaction, whose snapshot the workers would not see. */
static bool startable(int64_t workers) { return workers >= 1; }
static bool lifecycle_allowed(bool in_transaction) { return !in_transaction; }

/* Stopping releases the server once, however often it is asked. */
static bool stop(bool *running)
{
    *running = false;
    return true;
}

typedef struct inside {
    mt_atom *goal, *answer;
} inside;

/* Runs the guarded goal inside the transaction and rolls it back. */
static mt_status in_transaction(metta *m, void *user)
{
    inside *t = user;
    t->answer = mt_one(mt_eval(m, guarded(mt_keep(t->goal))));
    return MT_FAIL;
}

static mt_atom *transacted(metta *m, mt_atom *goal)
{
    inside t = { goal, NULL };
    require("the transaction ran", mt_transaction(m, in_transaction, &t) == MT_FAIL);
    mt_drop(goal);
    require("an answer inside it", t.answer != NULL);
    return t.answer;
}

static void route(metta *m, const char *path, mt_atom *response)
{
    require("a route", mt_add(m, E("=", E("library-http", E("http-request", V("method"), T(path), V("target"), V("fields"), V("body"))), response)));
}

int main(void)
{
    require("libcurl", curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK);
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_http", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_http")))));
    require("import lib_encoding", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_encoding")))));

    /* The routes, as equations over the request. */
    require("library-http's type", mt_add(m, E(":", "library-http", E("->", "Expression", "Expression"))));
    route(m, "/", E("http-response", 200, E(E(T("X-Reply"), T("one")), E(T("X-Reply"), T("two"))), E(0, 128, 255)));
    route(m, "/echo", E("http-response", 201,
                        E(E(T("Content-Type"), T("text/plain; charset=UTF-8")), E(T("X-Method"), E("repr", V("method"))),
                          E(T("X-Target"), V("target"))),
                        V("body")));
    route(m, "/absent", E("http-response", 404, mt_unit(), mt_unit()));
    route(m, "/redirect", E("http-response", 307, E(E(T("Location"), T("/"))), mt_unit()));
    route(m, "/empty", E("empty"));
    route(m, "/broken", E("broken-response", 7));
    require("read-http's type", mt_add(m, E(":", "read-http", E("->", "Expression", "Expression"))));
    require("read-http", mt_add(m, E("=", E("read-http", E("http-response", V("status"), V("fields"), V("handle"))),
                                     E("file-read-bytes!", V("handle")))));
    require("ping-http's type", mt_add(m, E(":", "ping-http", E("->", "Expression", "Expression"))));
    require("ping-http", mt_add(m, E("=", E("ping-http", V("server")), E("http-request!", "get", E("http-server-url", V("server")), mt_unit()))));

    mt_atom *names[METHODS];
    for (size_t i = 0; i < METHODS; i++) names[i] = S(methods[i]);
    assert(answers_are(mt_eval(m, E("http-methods")), E(mt_exprv(METHODS, names))) && "the methods");

    /* The server, and C's own copy of its URL. */
    mt_atom *server = value_of(m, E("http-server-start!", T("127.0.0.1"), 0, "library-http", E(E("workers", 2))));
    char url[64], echo[96], absent[80], empty[80], broken[80], redirect[80];
    snprintf(url, sizeof url, "http://%s:%lld/", mt_name(mt_at(server, 1)), (long long)mt_int(mt_at(server, 2)));
    snprintf(echo, sizeof echo, "%secho?q=one%%20two", url);
    snprintf(absent, sizeof absent, "%sabsent", url);
    snprintf(empty, sizeof empty, "%sempty", url);
    snprintf(broken, sizeof broken, "%sbroken", url);
    snprintf(redirect, sizeof redirect, "%sredirect", url);

    static const option five[] = { { .kind = TIMEOUT, .seconds = 5 } };
    request get = { "get", url, five, 1 };
    exchange x = fetched(&get);
    mt_atom *response = value_of(m, engine_request("http-request!", &get));
    const mt_atom *fields = mt_at(response, 2);
    assert(atom_is(mt_keep(mt_at(response, 1)), mt_num(x.status)) && "the status");
    assert(atom_is(mt_keep(mt_at(response, 3)), mt_array(x.n, x.bytes)) && "the bytes");
    assert(atom_is(value_of(m, E("http-header", mt_keep(fields), T("Content-Type"))), field_value(&x, "content-type")) && "the media type");
    assert(atom_is(value_of(m, E("http-header", mt_keep(fields), T("CONTENT-LENGTH"))), field_value(&x, "content-length")) && "a count, whatever the case");
    check_field(m, "a repeated field, in order", fields, "x-reply", &x);
    check_field(m, "a missing field", fields, "missing", &x);
    release(&x);

    /* HEAD: the metadata, and no bytes. */
    request head = { "head", url, NULL, 0 };
    x = fetched(&head);
    mt_atom *headed = value_of(m, engine_request("http-request!", &head));
    assert(atom_is(mt_keep(mt_at(headed, 1)), mt_num(x.status)) && "HEAD's status");
    assert(atom_is(mt_keep(mt_at(headed, 3)), mt_array(x.n, x.bytes)) && "no bytes");
    assert(atom_is(value_of(m, E("http-header", mt_keep(mt_at(headed, 2)), T("content-length"))), field_value(&x, "content-length")) && "but the count");
    release(&x);

    /* Text goes as its UTF-8 bytes, which a C string already is. */
    static const char text[] = "aπ🙂";
    int text_bytes[sizeof text - 1];
    for (size_t i = 0; i + 1 < sizeof text; i++) text_bytes[i] = (unsigned char)text[i];
    const option posting[] = {
        { .kind = BODY, .media = "text/plain; charset=UTF-8", .bytes = text_bytes, .n_bytes = sizeof text - 1 },
        { .kind = HEADER, .name = "X-Input", .value = "one" },
        { .kind = HEADER, .name = "X-Input", .value = "two" },
    };
    request post = { "post", echo, posting, 3 };
    x = fetched(&post);
    mt_atom *posted = value_of(m, engine_request("http-request!", &post));
    assert(atom_is(mt_keep(mt_at(posted, 1)), mt_num(x.status)) && "created");
    char *echoed = text_of(mt_at(posted, 3));
    assert(strcmp(echoed, text) == 0 && "the text comes back");
    free(echoed);
    assert(atom_is(value_of(m, E("http-header", mt_keep(mt_at(posted, 2)), T("x-method"))), field_value(&x, "x-method")) && "the method");
    assert(atom_is(value_of(m, E("http-header", mt_keep(mt_at(posted, 2)), T("x-target"))), field_value(&x, "x-target")) && "the raw target");
    release(&x);
    static const char *const others[] = { "put", "patch", "delete", "options" };
    for (size_t i = 0; i < 4; i++) {
        request other = { others[i], echo, NULL, 0 };
        x = fetched(&other);
        mt_atom *answered = value_of(m, engine_request("http-request!", &other));
        assert(atom_is(value_of(m, E("http-header", mt_keep(mt_at(answered, 2)), T("x-method"))), field_value(&x, "x-method")) && others[i]);
        mt_drop(answered);
        release(&x);
    }

    /* Statuses are data: an empty stream is 404, a malformed answer 500. */
    static const option following[] = { { .kind = REDIRECT, .follow = true } };
    const request statuses[] = { { "get", absent, NULL, 0 }, { "get", empty, NULL, 0 }, { "get", broken, NULL, 0 },
                                 { "get", redirect, NULL, 0 } };
    static const char *const status_claims[] = { "no route's status", "no answer's status", "a malformed answer's status",
                                                 "a redirect is not followed" };
    for (size_t i = 0; i < 4; i++) {
        x = fetched(&statuses[i]);
        mt_atom *answered = value_of(m, engine_request("http-request!", &statuses[i]));
        assert(atom_is(mt_keep(mt_at(answered, 1)), mt_num(x.status)) && status_claims[i]);
        mt_drop(answered);
        release(&x);
    }
    request followed = { "get", redirect, following, 1 };
    x = fetched(&followed);
    mt_atom *arrived = value_of(m, engine_request("http-request!", &followed));
    assert(atom_is(mt_keep(mt_at(arrived, 3)), mt_array(x.n, x.bytes)) && "unless asked");
    release(&x);

    /* The streaming door, held against the same bytes read in parts. */
    request plain = { "get", url, NULL, 0 };
    x = fetched(&plain);
    reader r = { x.bytes, x.n, 0, true };
    mt_atom *stream = value_of(m, engine_request("http-open!", &plain));
    const mt_atom *handle = mt_at(stream, 3);
    assert(atom_is(value_of(m, E("file-read-bytes!", mt_keep(handle), 2)), read_part(&r, 2)) && "two bytes");
    assert(atom_is(value_of(m, E("file-read-bytes!", mt_keep(handle))), read_part(&r, r.n)) && "the rest");
    assert(answers_are(mt_eval(m, E("file-close!", mt_keep(handle))), E(B(close_reader(&r)))) && "closed");
    assert(answers_are(mt_eval(m, E("file-close!", mt_keep(handle))), E(B(close_reader(&r)))) && "closed again");
    assert(answers_are(mt_eval(m, E("with-http", "get", T(url), mt_unit(), "read-http")), E(mt_array(x.n, x.bytes))) && "a scoped response");
    mt_atom *pinged = value_of(m, E("with-http-server", T("127.0.0.1"), 0, "library-http", E(E("workers", 1)), "ping-http"));
    assert(atom_is(mt_keep(mt_at(pinged, 1)), mt_num(x.status)) && "a scoped server");
    release(&x);

    /* Refusals come before any connection. */
    static const int past_a_byte[] = { 256 };
    static const option bad_name[] = { { .kind = HEADER, .name = "bad:name", .value = "x" } },
                        bad_value[] = { { .kind = HEADER, .name = "X-Input", .value = "a\nb" } },
                        bad_body[] = { { .kind = BODY, .media = "application/octet-stream", .bytes = past_a_byte, .n_bytes = 1 } },
                        twice[] = { { .kind = TIMEOUT, .seconds = 1 }, { .kind = TIMEOUT, .seconds = 2 } },
                        owned[] = { { .kind = HEADER, .name = "Content-Length", .value = "3" } };
    const struct {
        const char *claim;
        request r;
    } refusals[] = {
        { "an invented method", { "invented", url, NULL, 0 } },
        { "a file URL", { "get", "file:///etc/passwd", NULL, 0 } },
        { "a field name that is no token", { "get", url, bad_name, 1 } },
        { "a line break in a value", { "get", url, bad_value, 1 } },
        { "a byte past 255", { "post", url, bad_body, 1 } },
        { "a timeout given twice", { "get", url, twice, 2 } },
        { "a field the client owns", { "get", url, owned, 1 } },
    };
    for (size_t i = 0; i < sizeof refusals / sizeof *refusals; i++)
        assert(answers_are(mt_eval(m, guarded(engine_request("http-request!", &refusals[i].r))), E(verdict(acceptable(&refusals[i].r))))
               && refusals[i].claim);
    assert(answers_are(mt_eval(m, guarded(E("http-server-start!", T("127.0.0.1"), 0, "library-http", E(E("workers", 0))))), E(verdict(startable(0))))
           && "no workers");
    assert(atom_is(transacted(m, E("http-server-start!", T("127.0.0.1"), 0, "library-http", mt_unit())), verdict(lifecycle_allowed(true)))
           && "no start inside a transaction");
    assert(atom_is(transacted(m, E("http-server-stop!", mt_keep(server))), verdict(lifecycle_allowed(true))) && "no stop inside one either");

    /* Stopping twice is harmless. */
    bool running = true;
    assert(answers_are(mt_eval(m, E("http-server-stop!", mt_keep(server))), E(B(stop(&running)))) && "stopped");
    assert(answers_are(mt_eval(m, E("http-server-stop!", mt_keep(server))), E(B(stop(&running)))) && "stopped again");

    mt_atom *held[] = { server, response, headed, posted, arrived, stream, pinged };
    for (size_t i = 0; i < sizeof held / sizeof *held; i++) mt_drop(held[i]);
    mt_close(m);
    curl_global_cleanup();
    return 0;
}
#else
#include <stdio.h>

/* Without libcurl's headers the program only says what it needs. */
int main(void)
{
    fputs("38-http_lib.c needs libcurl: install its development files, then build with\n"
          "cc 38-http_lib.c $(pkg-config --cflags --libs cmetta libcurl)\n", stderr);
    return 77;
}
#endif
