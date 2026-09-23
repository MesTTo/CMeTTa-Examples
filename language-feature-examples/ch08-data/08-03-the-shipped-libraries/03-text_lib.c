/* Purpose: lib_string and lib_file, held against string.h and stdio. Each
 *   string operation's answer is the one C's own functions give on the same
 *   text: strlen, a clamped memcpy slice, a split and a join, isspace, toupper
 *   and tolower, strncmp, strstr, qsort with strcmp, strtoll and snprintf. For
 *   the files C reads what the library wrote with fopen, so every read the
 *   library answers is checked against the bytes on disk.
 * Guarantees: all thirty-seven claims of the original hold [tested: make
 *   twins; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"
#include <ctype.h>

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
    metta *m = open_engine();
    require("import lib_string", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_string")))));
    require("import lib_file", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_file")))));

    check_answers("string-length is strlen", mt_eval(m, E("string-length", T("hello"))), (int64_t)strlen("hello"));
    check_answers("a half-open slice", mt_eval(m, E("string-slice", T("hello world"), 0, 5)), slice("hello world", 0, 5));
    check_answers("an over-long end clamps", mt_eval(m, E("string-slice", T("hello"), 3, 999)), slice("hello", 3, 999));
    check_answers("so does a start past the end", mt_eval(m, E("string-slice", T("hello"), 99, 120)), slice("hello", 99, 120));
    check_answers("split", mt_eval(m, E("string-split", T(","), T("a,b,c"))), split(",", "a,b,c"));
    check_answers("join", mt_eval(m, E("string-join", T(", "), E(T("a"), T("b"), T("c")))), T("a, b, c"));
    check_answers("trim", mt_eval(m, E("string-trim", T("  padded  "))), trimmed("  padded  "));
    check_answers("upper is toupper", mt_eval(m, E("string-upper", T("shout"))), mapped("shout", toupper));
    check_answers("lower is tolower", mt_eval(m, E("string-lower", T("QUIET"))), mapped("QUIET", tolower));
    check_answers("starts-with is strncmp", mt_eval(m, E("string-starts-with", T("hello"), T("he"))), B(strncmp("hello", "he", 2) == 0));
    check_answers("ends-with", mt_eval(m, E("string-ends-with", T("hello"), T("lo"))), B(strcmp("hello" + 3, "lo") == 0));
    check_answers("contains is strstr", mt_eval(m, E("string-contains", T("hello"), T("ell"))), B(strstr("hello", "ell") != NULL));
    check_answers("and its absence", mt_eval(m, E("string-contains", T("hello"), T("zzz"))), B(strstr("hello", "zzz") != NULL));
    const char *hello = "hello", *l = strstr(hello, "l");
    check_answers("index-of is the strstr offset", mt_eval(m, E("string-index-of", T("hello"), T("l"))), (int64_t)(l - hello));
    check_answers("or -1", mt_eval(m, E("string-index-of", T("hello"), T("z"))), strstr(hello, "z") ? 0 : -1);
    check_answers("replace every occurrence", mt_eval(m, E("string-replace", T("banana"), T("a"), T("X"))), replaced("banana", "a", "X"));
    check_answers("chars are one-character texts", mt_eval(m, E("string-chars", T("abc"))), E(T("a"), T("b"), T("c")));
    check_answers("and join back", mt_eval(m, E("string-from-chars", E(T("a"), T("b"), T("c")))), T("abc"));
    check_answers("repeat", mt_eval(m, E("string-repeat", T("ab"), 3)), T("ababab"));
    check_answers("pad-left", mt_eval(m, E("string-pad-left", T("7"), 3, T("0"))), T("007"));
    check_answers("pad-right", mt_eval(m, E("string-pad-right", T("7"), 3, T("."))), T("7.."));
    check_answers("format-args fills in order", mt_eval(m, E("format-args", T("Probability of {} is {}%"), E("head", 50))),
                  formatted("Probability of {} is {}%", 2, (const char *const[]){ "head", "50" }));
    check_answers("and leaves a missing one empty", mt_eval(m, E("format-args", T("{} and {}"), E(T("only")))),
                  formatted("{} and {}", 1, (const char *const[]){ "only" }));
    const char *fruit[] = { "pear", "apple", "fig" };
    qsort(fruit, 3, sizeof *fruit, by_strcmp);
    check_answers("sort-strings is qsort with strcmp", mt_eval(m, E("sort-strings", E(T("pear"), T("apple"), T("fig")))),
                  E(T(fruit[0]), T(fruit[1]), T(fruit[2])));
    check_answers("parse-number is strtoll", mt_eval(m, E("parse-number", T("42"))), (int64_t)strtoll("42", NULL, 10));
    char digits[8];
    snprintf(digits, sizeof digits, "%d", 42);
    check_answers("number-to-string is snprintf", mt_eval(m, E("number-to-string", 42)), T(digits));
    check_answers("a symbol is text here", mt_eval(m, E("string-length", "hello")), (int64_t)strlen("hello"));
    check_answers("and upper-cases", mt_eval(m, E("string-upper", "hello")), mapped("hello", toupper));

    mt_atom *scratch = mt_one(mt_eval(m, E("temp-path!", T("metta-text-example"))));
    require("a fresh scratch path", scratch && mt_kind_of(scratch) == MT_TEXT);
    const char *path = mt_name(scratch);
    check_answers("write-file!", mt_eval(m, E("write-file!", mt_keep(scratch), T("one\ntwo\nthree\n"))), B(true));
    char *disk = slurp(path);
    check_answers("read-file! answers the bytes C reads", mt_eval(m, E("read-file!", mt_keep(scratch))), T(disk));
    check_answers("file-lines! is C's split into lines", mt_eval(m, E("file-lines!", mt_keep(scratch))), lines_of(disk));
    free(disk);
    check_answers("append-file!", mt_eval(m, E("append-file!", mt_keep(scratch), T("four\n"))), B(true));
    disk = slurp(path);
    check_answers("four lines now", mt_eval(m, E("file-lines!", mt_keep(scratch))), lines_of(disk));
    check_answers("a handle reads the first three bytes",
                  mt_eval(m, E("let", V("h"), E("file-open!", mt_keep(scratch), T("r")),
                               E("let", V("head"), E("file-read-exact!", V("h"), 3),
                                 E("let", V("_"), E("file-close!", V("h")), V("head"))))),
                  text_n(disk, 3));
    mt_atom *numbered = lines_of(disk), *rows[MOST];
    for (size_t i = 0; i < mt_len(numbered); i++) rows[i] = E((int64_t)i + 1, mt_keep(mt_at(numbered, i)));
    check_answers("file-space! numbers the lines",
                  mt_eval(m, E("let", V("log"), E("file-space!", mt_keep(scratch)),
                               E("collapse", E("match", V("log"), E("line", V("n"), V("t")), E(V("n"), V("t")))))),
                  mt_exprv(mt_len(numbered), rows));
    check_answers("one line is a match",
                  mt_eval(m, E("let", V("log"), E("file-space!", mt_keep(scratch)),
                               E("collapse", E("match", V("log"), E("line", 2, V("t")), V("t"))))),
                  E(mt_keep(mt_at(numbered, 1))));
    mt_drop(numbered);
    free(disk);
    check_answers("delete-file!", mt_eval(m, E("delete-file!", mt_keep(scratch))), B(true));
    FILE *gone = fopen(path, "r");
    check("and C finds it gone", gone == NULL);
    if (gone) fclose(gone);
    mt_drop(scratch);
    return done(m);
}
