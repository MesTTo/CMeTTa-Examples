/* Purpose: lib_string and lib_file, held against string.h and stdio. Each
 *   string operation's answer is the one C's own functions give on the same
 *   text: strlen, a clamped memcpy slice, a split and a join, isspace, toupper
 *   and tolower, strncmp, strstr, qsort with strcmp, strtoll and snprintf. For
 *   the files C reads what the library wrote with fopen, so every read the
 *   library answers is checked against the bytes on disk.
 * Guarantees: all thirty-seven claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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

enum { MOST = 64 };

static mt_atom *text_n(const char *s, size_t n) { return mt_textn(s, n); }

static mt_atom *slice(const char *s, size_t from, size_t to)
{
    size_t n = strlen(s);
    from = from < n ? from : n;
    to = to < n ? to : n;
    return text_n(s + from, to > from ? to - from : 0);
}

/* The pieces of `s` between each `sep`, as texts. */
static mt_atom *split(const char *sep, const char *s)
{
    mt_atom *pieces[MOST];
    size_t n = 0, width = strlen(sep);
    const char *at;
    while ((at = strstr(s, sep)) != NULL) {
        pieces[n++] = text_n(s, (size_t)(at - s));
        s = at + width;
    }
    pieces[n++] = T(s);
    return mt_exprv(n, pieces);
}

static mt_atom *mapped(const char *s, int (*f)(int))
{
    char out[MOST];
    size_t n = strlen(s);
    for (size_t i = 0; i < n; i++) out[i] = (char)f((unsigned char)s[i]);
    return text_n(out, n);
}

static mt_atom *trimmed(const char *s)
{
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1])) n--;
    while (n && isspace((unsigned char)*s)) s++, n--;
    return text_n(s, n);
}

static mt_atom *replaced(const char *s, const char *from, const char *to)
{
    char out[MOST] = "";
    const char *at;
    size_t width = strlen(from);
    while ((at = strstr(s, from)) != NULL) {
        strncat(out, s, (size_t)(at - s));
        strcat(out, to);
        s = at + width;
    }
    strcat(out, s);
    return T(out);
}

/* `{}` placeholders filled in order; a missing argument fills nothing. */
static mt_atom *formatted(const char *pattern, size_t argc, const char *const *argv)
{
    char out[MOST] = "";
    const char *at;
    size_t used = 0;
    while ((at = strstr(pattern, "{}")) != NULL) {
        strncat(out, pattern, (size_t)(at - pattern));
        if (used < argc) strcat(out, argv[used++]);
        pattern = at + 2;
    }
    strcat(out, pattern);
    return T(out);
}

static int by_strcmp(const void *a, const void *b) { return strcmp(*(const char *const *)a, *(const char *const *)b); }

/* A file's bytes, as C reads them. */
static char *slurp(const char *path)
{
    FILE *f = fopen(path, "rb");
    char *data = f ? calloc(1, 4096) : NULL;
    if (data) data[fread(data, 1, 4095, f)] = '\0';
    if (f) fclose(f);
    return data;
}

/* The lines of a text, a final newline ending the last rather than
   opening an empty one. */
static mt_atom *lines_of(const char *data)
{
    mt_atom *lines[MOST];
    size_t n = 0;
    const char *at;
    while (*data && (at = strchr(data, '\n')) != NULL) {
        lines[n++] = text_n(data, (size_t)(at - data));
        data = at + 1;
    }
    if (*data) lines[n++] = T(data);
    return mt_exprv(n, lines);
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_string", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_string")))));
    require("import lib_file", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_file")))));

    assert(answers_are(mt_eval(m, E("string-length", T("hello"))), E((int64_t)strlen("hello"))) && "string-length is strlen");
    assert(answers_are(mt_eval(m, E("string-slice", T("hello world"), 0, 5)), E(slice("hello world", 0, 5))) && "a half-open slice");
    assert(answers_are(mt_eval(m, E("string-slice", T("hello"), 3, 999)), E(slice("hello", 3, 999))) && "an over-long end clamps");
    assert(answers_are(mt_eval(m, E("string-slice", T("hello"), 99, 120)), E(slice("hello", 99, 120))) && "so does a start past the end");
    assert(answers_are(mt_eval(m, E("string-split", T(","), T("a,b,c"))), E(split(",", "a,b,c"))) && "split");
    assert(answers_are(mt_eval(m, E("string-join", T(", "), E(T("a"), T("b"), T("c")))), E(T("a, b, c"))) && "join");
    assert(answers_are(mt_eval(m, E("string-trim", T("  padded  "))), E(trimmed("  padded  "))) && "trim");
    assert(answers_are(mt_eval(m, E("string-upper", T("shout"))), E(mapped("shout", toupper))) && "upper is toupper");
    assert(answers_are(mt_eval(m, E("string-lower", T("QUIET"))), E(mapped("QUIET", tolower))) && "lower is tolower");
    assert(answers_are(mt_eval(m, E("string-starts-with", T("hello"), T("he"))), E(B(strncmp("hello", "he", 2) == 0))) && "starts-with is strncmp");
    assert(answers_are(mt_eval(m, E("string-ends-with", T("hello"), T("lo"))), E(B(strcmp("hello" + 3, "lo") == 0))) && "ends-with");
    assert(answers_are(mt_eval(m, E("string-contains", T("hello"), T("ell"))), E(B(strstr("hello", "ell") != NULL))) && "contains is strstr");
    assert(answers_are(mt_eval(m, E("string-contains", T("hello"), T("zzz"))), E(B(strstr("hello", "zzz") != NULL))) && "and its absence");
    const char *hello = "hello", *l = strstr(hello, "l");
    assert(answers_are(mt_eval(m, E("string-index-of", T("hello"), T("l"))), E((int64_t)(l - hello))) && "index-of is the strstr offset");
    assert(answers_are(mt_eval(m, E("string-index-of", T("hello"), T("z"))), E(strstr(hello, "z") ? 0 : -1)) && "or -1");
    assert(answers_are(mt_eval(m, E("string-replace", T("banana"), T("a"), T("X"))), E(replaced("banana", "a", "X"))) && "replace every occurrence");
    assert(answers_are(mt_eval(m, E("string-chars", T("abc"))), E(E(T("a"), T("b"), T("c")))) && "chars are one-character texts");
    assert(answers_are(mt_eval(m, E("string-from-chars", E(T("a"), T("b"), T("c")))), E(T("abc"))) && "and join back");
    assert(answers_are(mt_eval(m, E("string-repeat", T("ab"), 3)), E(T("ababab"))) && "repeat");
    assert(answers_are(mt_eval(m, E("string-pad-left", T("7"), 3, T("0"))), E(T("007"))) && "pad-left");
    assert(answers_are(mt_eval(m, E("string-pad-right", T("7"), 3, T("."))), E(T("7.."))) && "pad-right");
    assert(answers_are(mt_eval(m, E("format-args", T("Probability of {} is {}%"), E("head", 50))), E(formatted("Probability of {} is {}%", 2, (const char *const[]){ "head", "50" })))
           && "format-args fills in order");
    assert(answers_are(mt_eval(m, E("format-args", T("{} and {}"), E(T("only")))), E(formatted("{} and {}", 1, (const char *const[]){ "only" })))
           && "and leaves a missing one empty");
    const char *fruit[] = { "pear", "apple", "fig" };
    qsort(fruit, 3, sizeof *fruit, by_strcmp);
    assert(answers_are(mt_eval(m, E("sort-strings", E(T("pear"), T("apple"), T("fig")))), E(E(T(fruit[0]), T(fruit[1]), T(fruit[2]))))
           && "sort-strings is qsort with strcmp");
    assert(answers_are(mt_eval(m, E("parse-number", T("42"))), E((int64_t)strtoll("42", NULL, 10))) && "parse-number is strtoll");
    char digits[8];
    snprintf(digits, sizeof digits, "%d", 42);
    assert(answers_are(mt_eval(m, E("number-to-string", 42)), E(T(digits))) && "number-to-string is snprintf");
    assert(answers_are(mt_eval(m, E("string-length", "hello")), E((int64_t)strlen("hello"))) && "a symbol is text here");
    assert(answers_are(mt_eval(m, E("string-upper", "hello")), E(mapped("hello", toupper))) && "and upper-cases");

    mt_atom *scratch = mt_one(mt_eval(m, E("temp-path!", T("metta-text-example"))));
    require("a fresh scratch path", scratch && mt_kind_of(scratch) == MT_TEXT);
    const char *path = mt_name(scratch);
    assert(answers_are(mt_eval(m, E("write-file!", mt_keep(scratch), T("one\ntwo\nthree\n"))), E(B(true))) && "write-file!");
    char *disk = slurp(path);
    assert(answers_are(mt_eval(m, E("read-file!", mt_keep(scratch))), E(T(disk))) && "read-file! answers the bytes C reads");
    assert(answers_are(mt_eval(m, E("file-lines!", mt_keep(scratch))), E(lines_of(disk))) && "file-lines! is C's split into lines");
    free(disk);
    assert(answers_are(mt_eval(m, E("append-file!", mt_keep(scratch), T("four\n"))), E(B(true))) && "append-file!");
    disk = slurp(path);
    assert(answers_are(mt_eval(m, E("file-lines!", mt_keep(scratch))), E(lines_of(disk))) && "four lines now");
    assert(answers_are(mt_eval(m, E("let", V("h"), E("file-open!", mt_keep(scratch), T("r")),
                                    E("let", V("head"), E("file-read-exact!", V("h"), 3),
                                      E("let", V("_"), E("file-close!", V("h")), V("head"))))), E(text_n(disk, 3)))
           && "a handle reads the first three bytes");
    mt_atom *numbered = lines_of(disk), *rows[MOST];
    for (size_t i = 0; i < mt_len(numbered); i++) rows[i] = E((int64_t)i + 1, mt_keep(mt_at(numbered, i)));
    assert(answers_are(mt_eval(m, E("let", V("log"), E("file-space!", mt_keep(scratch)),
                                    E("collapse", E("match", V("log"), E("line", V("n"), V("t")), E(V("n"), V("t")))))), E(mt_exprv(mt_len(numbered), rows)))
           && "file-space! numbers the lines");
    assert(answers_are(mt_eval(m, E("let", V("log"), E("file-space!", mt_keep(scratch)),
                                    E("collapse", E("match", V("log"), E("line", 2, V("t")), V("t"))))), E(E(mt_keep(mt_at(numbered, 1)))))
           && "one line is a match");
    mt_drop(numbered);
    free(disk);
    assert(answers_are(mt_eval(m, E("delete-file!", mt_keep(scratch))), E(B(true))) && "delete-file!");
    FILE *gone = fopen(path, "r");
    assert(gone == NULL && "and C finds it gone");
    if (gone) fclose(gone);
    mt_drop(scratch);
    mt_close(m);
    return 0;
}
