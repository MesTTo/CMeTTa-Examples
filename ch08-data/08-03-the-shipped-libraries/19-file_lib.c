/* Purpose: lib_file, held against POSIX C doing the same jobs on the same
 *   paths. What a file should hold is C's own model of what it asked the
 *   engine to write; each engine handle is shadowed by a FILE* of C's own on
 *   the same file, which reads, seeks and outlives a rename the same way; and
 *   the tree is observed with the system calls: lstat for a kind, stat for
 *   identity, readlink, readdir sorted by codepoint, fts(3) for the walk,
 *   realpath(3) for resolution. A glob is CPython's selector structure, which
 *   the library follows, written in C over readdir and fnmatch(3): a literal
 *   component joins without listing, a wildcard filters one directory, **
 *   descends, and a brace group expands the shell's way before fnmatch sees
 *   it. Lexical paths are posixpath's rules as C string code, and C builds
 *   every path itself with the join rule path-join follows. Where the library
 *   refuses, a precondition C states refuses the same input.
 * Assumes: glibc's fts.h; the working directory is the engine tree.
 * Owns resources: everything either side creates lives under the engine's
 *   temporary directory, which the last claims delete; C closes its FILE*s
 *   and removes the scratch directory it makes itself.
 * Guarantees: all ninety claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define _XOPEN_SOURCE 700 /* realpath is X/Open's in glibc */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <fnmatch.h>
#include <fts.h>
#include <libgen.h>
#include <limits.h>
#include <sys/stat.h>
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

enum { MOST = 32, SLOTS = 16 };

/* A path joined the way path-join joins, SWI's directory_file_path: an
   absolute name stands alone, and one separator goes between. The result
   lives in one of SLOTS buffers, reused in turn. */
static const char *under(const char *dir, const char *name)
{
    static char slot[SLOTS][PATH_MAX];
    static size_t next;
    char *out = slot[next++ % SLOTS];
    size_t len = strlen(dir);
    if (*name == '/') snprintf(out, PATH_MAX, "%s", name);
    else snprintf(out, PATH_MAX, "%s%s%s", dir, len && dir[len - 1] == '/' ? "" : "/", name);
    return out;
}

/* Observations. */
static bool is_dir(const char *path)
{
    struct stat s;
    return stat(path, &s) == 0 && S_ISDIR(s.st_mode);
}

static bool is_file(const char *path)
{
    struct stat s;
    return stat(path, &s) == 0 && S_ISREG(s.st_mode);
}

static mt_atom *kind(const char *path)
{
    struct stat s;
    if (lstat(path, &s) != 0) return S("missing");
    return S(S_ISLNK(s.st_mode) ? "link" : S_ISDIR(s.st_mode) ? "directory" : S_ISREG(s.st_mode) ? "file" : "other");
}

static bool same_file(const char *a, const char *b)
{
    struct stat x, y;
    return stat(a, &x) == 0 && stat(b, &y) == 0 && x.st_dev == y.st_dev && x.st_ino == y.st_ino;
}

static int64_t size_of(const char *path)
{
    struct stat s;
    require("stat", stat(path, &s) == 0);
    return (int64_t)s.st_size;
}

static mt_atom *link_text(const char *path)
{
    char target[PATH_MAX];
    ssize_t n = readlink(path, target, sizeof target);
    require("readlink", n >= 0);
    return mt_textn(target, (size_t)n);
}

static int by_name(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

/* A directory's names less . and .., in codepoint order, which for UTF-8 is
   byte order. */
typedef struct names {
    char *at[MOST];
    size_t n;
} names;

static names listing(const char *dir)
{
    names out = { .n = 0 };
    DIR *d = opendir(dir);
    require("opendir", d != NULL);
    for (struct dirent *e; (e = readdir(d));) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        require("room for the names", out.n < MOST);
        out.at[out.n++] = strdup(e->d_name);
    }
    closedir(d);
    qsort(out.at, out.n, sizeof *out.at, by_name);
    return out;
}

static void names_free(names *list)
{
    while (list->n) free(list->at[--list->n]);
}

static mt_atom *texts(char *const *at, size_t n)
{
    mt_atom *kids[MOST];
    for (size_t i = 0; i < n; i++) kids[i] = T(at[i]);
    return mt_exprv(n, kids);
}

static mt_atom *listed(const char *dir)
{
    names list = listing(dir);
    mt_atom *out = texts(list.at, list.n);
    names_free(&list);
    return out;
}

static int by_entry(const FTSENT **a, const FTSENT **b) { return strcmp((*a)->fts_name, (*b)->fts_name); }

/* Every descendant, depth first, names in codepoint order, by fts(3). A
   physical walk reports a link and stays out; a logical one enters it, and
   fts reports a directory already on the current chain as FTS_DC without
   entering it, the library's cycle rule. */
static mt_atom *walked(const char *root, bool follow)
{
    char *roots[] = { (char *)root, NULL }, *found[MOST];
    size_t n = 0;
    FTS *tree = fts_open(roots, (follow ? FTS_LOGICAL : FTS_PHYSICAL) | FTS_NOCHDIR, by_entry);
    require("fts_open", tree != NULL);
    for (FTSENT *e; (e = fts_read(tree));) {
        if (e->fts_level == FTS_ROOTLEVEL || e->fts_info == FTS_DP) continue;
        require("room for the walk", n < MOST);
        found[n++] = strdup(e->fts_path);
    }
    fts_close(tree);
    mt_atom *out = texts(found, n);
    while (n) free(found[--n]);
    return out;
}

/* A glob in CPython's selector structure, which dir-glob follows [source:
   lib/lib_file/lib_file.pl, glob_select/7; commit=33c2d50c84b24c1a2c906225600e2a4ffdb662f1].
   A component's grammar: \ escapes, [...] is a bracket expression, {a,b}
   alternates, and fnmatch(3) reads what is left. */
typedef struct glob {
    bool hidden, follow, malformed;
    char *found[MOST];
    size_t n;
} glob;

/* Where the bracket expression opening at s ends, past its ], or NULL when
   it never closes; a ] right after [ or [! is a member. */
static const char *bracket_end(const char *s)
{
    const char *p = s + 1;
    p += *p == '!' || *p == '^';
    p += *p == ']';
    while (*p && *p != ']') p++;
    return *p ? p + 1 : NULL;
}

/* The next of `stops` outside escapes and bracket expressions, or NULL; an
   unclosed bracket makes the component malformed. */
static const char *scan_to(glob *g, const char *s, const char *stops)
{
    for (; *s; s++) {
        if (*s == '\\' && s[1]) s++;
        else if (*s == '[') {
            const char *end = bracket_end(s);
            if (!end) return g->malformed = true, NULL;
            s = end - 1;
        } else if (strchr(stops, *s))
            return s;
    }
    return NULL;
}

/* Whether name matches the pattern, its first brace group expanded into
   each alternative in turn, the shell's rule. */
static bool matches(glob *g, const char *pattern, const char *name)
{
    const char *open = scan_to(g, pattern, "{");
    if (g->malformed) return false;
    if (!open) return fnmatch(pattern, name, 0) == 0;
    const char *commas[MOST], *p = open + 1;
    size_t alternatives = 0, depth = 0;
    for (;; p++) {
        if (!(p = scan_to(g, p, "{,}"))) return g->malformed = true, false;
        if (*p == '{') depth++;
        else if (*p == '}' && depth) depth--;
        else if (depth == 0) {
            require("room for the alternatives", alternatives < MOST);
            commas[alternatives++] = p;
            if (*p == '}') break;
        }
    }
    for (size_t i = 0; i < alternatives; i++) {
        const char *from = i ? commas[i - 1] + 1 : open + 1;
        char one[PATH_MAX];
        snprintf(one, sizeof one, "%.*s%.*s%s", (int)(open - pattern), pattern, (int)(commas[i] - from), from, p + 1);
        if (matches(g, one, name)) return true;
    }
    return false;
}

static bool wildcard(const char *component) { return strpbrk(component, "*?[{\\") != NULL; }

/* The dotfile rule: a wildcard skips a leading dot unless the component
   starts with one or hidden names are asked for. */
static bool visible(const glob *g, const char *component, const char *name)
{
    return g->hidden || *component == '.' || *name != '.';
}

/* Whether a descent enters path: a directory always, a link only when
   following, to a directory not already on the chain. */
static bool enters(const glob *g, const char *path, const struct stat *chain, size_t depth)
{
    struct stat l, s;
    if (lstat(path, &l) != 0) return false;
    if (!S_ISLNK(l.st_mode)) return S_ISDIR(l.st_mode);
    if (!g->follow || stat(path, &s) != 0 || !S_ISDIR(s.st_mode)) return false;
    for (size_t i = 0; i < depth; i++)
        if (chain[i].st_dev == s.st_dev && chain[i].st_ino == s.st_ino) return false;
    return true;
}

static void found(glob *g, const char *path)
{
    struct stat s;
    if (stat(path, &s) != 0) return;
    for (size_t i = 0; i < g->n; i++)
        if (strcmp(g->found[i], path) == 0) return;
    require("room for the matches", g->n < MOST);
    g->found[g->n++] = strdup(path);
}

static void select_from(glob *g, char *const *component, size_t count, const char *current, struct stat *chain, size_t depth);

static void step(glob *g, char *const *component, size_t count, const char *sub, struct stat *chain, size_t depth)
{
    if (count == 1) found(g, sub);
    else if (is_dir(sub) && depth < MOST && stat(sub, &chain[depth]) == 0)
        select_from(g, component + 1, count - 1, sub, chain, depth + 1);
}

static void select_from(glob *g, char *const *component, size_t count, const char *current, struct stat *chain, size_t depth)
{
    if (!count) {
        found(g, current);
        return;
    }
    if (!wildcard(component[0])) {
        step(g, component, count, under(current, component[0]), chain, depth);
        return;
    }
    bool descend = strcmp(component[0], "**") == 0;
    if (descend) select_from(g, component + 1, count - 1, current, chain, depth);
    names list = listing(current);
    for (size_t i = 0; i < list.n && !g->malformed; i++) {
        if (!visible(g, component[0], list.at[i])) continue;
        char sub[PATH_MAX];
        snprintf(sub, sizeof sub, "%s", under(current, list.at[i]));
        if (!descend && matches(g, component[0], list.at[i])) step(g, component, count, sub, chain, depth);
        else if (descend && depth < MOST && enters(g, sub, chain, depth) && stat(sub, &chain[depth]) == 0)
            select_from(g, component, count, sub, chain, depth + 1);
    }
    names_free(&list);
}

/* Every match of a relative pattern: its components with empty ones and .
   dropped and a run of ** read as one. NULL when a component is malformed. */
static mt_atom *globbed(const char *root, const char *pattern, bool hidden, bool follow)
{
    char copy[PATH_MAX], *component[MOST], *save;
    size_t count = 0;
    snprintf(copy, sizeof copy, "%s", pattern);
    for (char *c = strtok_r(copy, "/", &save); c; c = strtok_r(NULL, "/", &save)) {
        bool again = count && strcmp(c, "**") == 0 && strcmp(component[count - 1], "**") == 0;
        if (strcmp(c, ".") == 0 || again) continue;
        require("room for the components", count < MOST);
        component[count++] = c;
    }
    glob g = { .hidden = hidden, .follow = follow };
    struct stat chain[MOST];
    require("the root is a directory", stat(root, &chain[0]) == 0);
    select_from(&g, component, count, root, chain, 1);
    mt_atom *out = g.malformed ? NULL : texts(g.found, g.n);
    while (g.n) free(g.found[--g.n]);
    return out;
}

/* Lexical paths, posixpath's rules [source: lib/lib_file/lib_file.pl,
   normalize_text/2, absolute_text/2 and 'path-relative'/3, each citing
   CPython's Lib/posixpath.py; commit=33c2d50c84b24c1a2c906225600e2a4ffdb662f1]:
   exactly two leading slashes are a root of their own, any other run is /,
   and empty and . components drop out. */
typedef struct parts {
    char buf[PATH_MAX];
    const char *root;
    char *at[MOST];
    size_t n;
} parts;

static void split_path(const char *path, parts *p)
{
    size_t slashes = strspn(path, "/");
    char *save;
    p->root = slashes == 2 ? "//" : slashes ? "/" : "";
    p->n = 0;
    snprintf(p->buf, sizeof p->buf, "%s", path + slashes);
    for (char *c = strtok_r(p->buf, "/", &save); c; c = strtok_r(NULL, "/", &save))
        if (strcmp(c, ".") != 0) {
            require("room for the components", p->n < MOST);
            p->at[p->n++] = c;
        }
}

static void joined(char *out, size_t size, const char *root, char *const *at, size_t n)
{
    size_t used = (size_t)snprintf(out, size, "%s", root);
    for (size_t i = 0; i < n; i++) used += (size_t)snprintf(out + used, size - used, "%s%s", i ? "/" : "", at[i]);
    if (!*out) snprintf(out, size, ".");
}

/* A .. stays at the start of a relative path or after another .., pops a
   component otherwise, and is dropped at the root. */
static void normalize(const char *path, char *out, size_t size)
{
    parts p;
    split_path(path, &p);
    size_t n = 0;
    for (size_t i = 0; i < p.n; i++) {
        bool up = strcmp(p.at[i], "..") == 0;
        if (up && n && strcmp(p.at[n - 1], "..") != 0) n--;
        else if (!up || !*p.root) p.at[n++] = p.at[i];
    }
    joined(out, size, p.root, p.at, n);
}

static mt_atom *normalized(const char *path)
{
    char out[PATH_MAX];
    normalize(path, out, sizeof out);
    return T(out);
}

static void absolute(const char *path, char *out, size_t size)
{
    char cwd[PATH_MAX];
    require("getcwd", getcwd(cwd, sizeof cwd) != NULL);
    normalize(*path == '/' ? path : under(cwd, path), out, size);
}

static mt_atom *absolute_of(const char *path)
{
    char out[PATH_MAX];
    absolute(path, out, sizeof out);
    return T(out);
}

static mt_atom *relative(const char *path, const char *start)
{
    char a[PATH_MAX], b[PATH_MAX], out[PATH_MAX];
    parts to, from;
    absolute(path, a, sizeof a);
    absolute(start, b, sizeof b);
    split_path(a, &to);
    split_path(b, &from);
    size_t common = 0, n = 0;
    while (common < to.n && common < from.n && strcmp(to.at[common], from.at[common]) == 0) common++;
    char *rel[2 * MOST], up[] = "..";
    for (size_t i = common; i < from.n; i++) rel[n++] = up;
    for (size_t i = common; i < to.n; i++) rel[n++] = to.at[i];
    joined(out, sizeof out, "", rel, n);
    return T(out);
}

static mt_atom *path_parts(const char *path)
{
    parts p;
    split_path(path, &p);
    mt_atom *kids[MOST + 1];
    size_t n = 0;
    if (*p.root) kids[n++] = T(p.root);
    for (size_t i = 0; i < p.n; i++) kids[n++] = T(p.at[i]);
    return mt_exprv(n, kids);
}

/* dirname(3) and basename(3) edit their argument, so each gets a copy. */
static mt_atom *parent(const char *path)
{
    char copy[PATH_MAX];
    snprintf(copy, sizeof copy, "%s", path);
    return T(dirname(copy));
}

static mt_atom *name_of(const char *path)
{
    char copy[PATH_MAX];
    snprintf(copy, sizeof copy, "%s", path);
    return T(basename(copy));
}

/* The extension is what follows the name's last dot, which SWI's
   file_name_extension/3 reads the same way, so .env has extension env. */
static mt_atom *extension(const char *path, bool stem)
{
    char copy[PATH_MAX];
    snprintf(copy, sizeof copy, "%s", path);
    const char *name = basename(copy), *dot = strrchr(name, '.');
    if (!dot) return T(stem ? name : "");
    return stem ? mt_textn(name, (size_t)(dot - name)) : T(dot + 1);
}

static mt_atom *resolved(const char *path)
{
    char out[PATH_MAX];
    require("realpath", realpath(path, out) != NULL);
    return T(out);
}

/* Reading through C's own handle. */
static mt_atom *text_read(FILE *f, size_t most)
{
    char buf[256];
    size_t n = fread(buf, 1, most < sizeof buf ? most : sizeof buf, f);
    return mt_textn(buf, n);
}

static mt_atom *octets_read(FILE *f, size_t most)
{
    unsigned char buf[MOST];
    return mt_array(fread(buf, 1, most < sizeof buf ? most : sizeof buf, f), buf);
}

/* A text model's LF-separated lines, one terminal empty line dropped. */
static mt_atom *lines_of(const char *text, size_t only)
{
    mt_atom *kids[MOST];
    size_t n = 0;
    for (const char *s = text; *s;) {
        size_t len = strcspn(s, "\n");
        require("room for the lines", n < MOST);
        kids[n++] = mt_textn(s, len);
        s += len + (s[len] == '\n');
    }
    if (!only) return mt_exprv(n, kids);
    require("the line is there", only <= n);
    mt_atom *line = kids[only - 1];
    for (size_t i = 0; i < n; i++)
        if (i != only - 1) mt_drop(kids[i]);
    return line;
}

/* What if-error answers: refused when C's precondition fails. */
static mt_atom *verdict(bool holds) { return S(holds ? "fine" : "refused"); }
static mt_atom *guarded(mt_atom *goal) { return E("if-error", E("catch", goal), "refused", "fine"); }

/* Whether path lies strictly inside dir: a directory cannot become its own
   descendant, by copy or by rename(2), which fails with EINVAL there. */
static bool inside(const char *path, const char *dir)
{
    size_t n = strlen(dir);
    return strncmp(path, dir, n) == 0 && path[n] == '/';
}

static bool bytes_only(const int64_t *at, size_t n)
{
    for (size_t i = 0; i < n; i++)
        if (at[i] < 0 || at[i] > 255) return false;
    return true;
}

static mt_atom *value_of(metta *m, mt_atom *goal)
{
    mt_atom *v = mt_one(mt_eval(m, goal));
    require("a value", v != NULL);
    return v;
}

static mt_atom *collapsed(mt_atom *goal) { return E("collapse", goal); }

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_file", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_file")))));
    /* The functions the scopes apply: size-then-two answers twice, so its
       equations are terms C builds and adds. */
    require("size-then-two answers the size", mt_add(m, E("=", E("size-then-two", V("h")), E("file-get-size!", V("h")))));
    require("and then two characters", mt_add(m, E("=", E("size-then-two", V("h")), E("file-read-exact!", V("h"), 2))));
    require("fill-and-list", mt_add(m, E("=", E("fill-and-list", V("name"), V("work")),
                                         E("let*", E(E(V("written"), E("write-file!", E("path-join", V("work"), V("name")), V("name")))),
                                           E("list-dir!", V("work"))))));
    require("close-then-read", mt_add(m, E("=", E("close-then-read", V("h")),
                                           E("let*", E(E(V("closed"), E("file-close!", V("h")))), E("file-read-to-string!", V("h"))))));

    mt_atom *minted = value_of(m, E("temp-dir!", T("file-lib")));
    char dir[PATH_MAX];
    snprintf(dir, sizeof dir, "%s", mt_name(minted));
    mt_drop(minted);
    assert(answers_are(mt_eval(m, E("dir-exists", T(dir))), E(B(is_dir(dir)))) && "the directory exists");
    assert(answers_are(mt_eval(m, E("file-kind", T(dir))), E(kind(dir))) && "and is a directory");

    /* Text files, checked against what C asked to be written. */
    char notes[PATH_MAX], model[128] = "";
    snprintf(notes, sizeof notes, "%s", under(dir, "notes.txt"));
    assert(answers_are(mt_eval(m, E("write-file!", T(notes), T("alpha\nbeta\n"))), E(B(true))) && "write-file!");
    strcat(model, "alpha\nbeta\n");
    assert(answers_are(mt_eval(m, E("append-file!", T(notes), T("gamma\n"))), E(B(true))) && "append-file!");
    strcat(model, "gamma\n");
    assert(answers_are(mt_eval(m, E("read-file!", T(notes))), E(T(model))) && "read-file!");
    assert(answers_are(mt_eval(m, E("file-lines!", T(notes))), E(lines_of(model, 0))) && "file-lines!");
    mt_atom *lines = value_of(m, E("file-space!", T(notes)));
    assert(answers_are(mt_eval(m, E("match", mt_keep(lines), E("line", 2, V("text")), V("text"))), E(lines_of(model, 2)))
           && "a file's lines as a space");
    mt_drop(lines);
    assert(answers_are(mt_eval(m, E("file-exists", T(notes))), E(B(is_file(notes)))) && "file-exists");
    assert(answers_are(mt_eval(m, E("file-kind", T(notes))), E(kind(notes))) && "a file's kind");

    /* Bytes: C's octets, whole-file and through a handle C shadows. */
    char blob[PATH_MAX];
    snprintf(blob, sizeof blob, "%s", under(dir, "blob.bin"));
    static const unsigned char four[] = { 0, 1, 255, 10 }, five[] = { 0, 1, 255, 10, 7 }, seven[] = { 7 };
    assert(answers_are(mt_eval(m, E("write-bytes!", T(blob), mt_array(4, four))), E(B(true))) && "write-bytes!");
    assert(answers_are(mt_eval(m, E("append-bytes!", T(blob), mt_array(1, seven))), E(B(true))) && "append-bytes!");
    assert(answers_are(mt_eval(m, E("read-bytes!", T(blob))), E(mt_array(5, five))) && "read-bytes!");
    mt_atom *h = value_of(m, E("file-open!", T(blob), T("rb")));
    FILE *f = fopen(blob, "rb");
    require("C opens the same file", f != NULL);
    assert(answers_are(mt_eval(m, E("file-read-bytes!", mt_keep(h), 2)), E(octets_read(f, 2))) && "a binary handle reads two octets");
    assert(answers_are(mt_eval(m, E("file-read-bytes!", mt_keep(h))), E(octets_read(f, SIZE_MAX))) && "and then the rest");
    fclose(f);
    require("file-close!", mt_one_truth(mt_eval(m, E("file-close!", h))));
    static const unsigned char ab[] = { 65, 66 };
    h = value_of(m, E("file-open!", T(blob), T("wb")));
    require("file-write-bytes!", mt_one_truth(mt_eval(m, E("file-write-bytes!", mt_keep(h), mt_array(2, ab)))));
    require("file-close!", mt_one_truth(mt_eval(m, E("file-close!", h))));
    assert(answers_are(mt_eval(m, E("read-file!", T(blob))), E(mt_textn((const char *)ab, 2))) && "a wb handle leaves what it wrote");
    const char *text_mode = "r";
    assert(answers_are(mt_eval(m, guarded(E("with-file", T(blob), T(text_mode), E("|->", E(V("h")), E("file-read-bytes!", V("h")))))), E(verdict(strchr(text_mode, 'b') != NULL)))
           && "octets need a binary handle");
    static const int64_t wide[] = { 1, 300 };
    assert(answers_are(mt_eval(m, guarded(E("write-bytes!", T(blob), E(wide[0], wide[1])))), E(verdict(bytes_only(wide, 2))))
           && "an octet is at most 255");

    /* The handle surface, each step beside the same step on C's FILE*. */
    h = value_of(m, E("file-open!", T(notes), T("r")));
    f = fopen(notes, "r");
    require("C opens the notes", f != NULL);
    assert(answers_are(mt_eval(m, E("file-get-size!", mt_keep(h))), E(size_of(notes))) && "a handle's size");
    assert(answers_are(mt_eval(m, E("file-read-exact!", mt_keep(h), 5)), E(text_read(f, 5))) && "an exact read");
    require("file-seek!", mt_one_truth(mt_eval(m, E("file-seek!", mt_keep(h), 6))) && fseek(f, 6, SEEK_SET) == 0);
    assert(answers_are(mt_eval(m, E("file-read-exact!", mt_keep(h), 4)), E(text_read(f, 4))) && "a read after a seek");
    assert(answers_are(mt_eval(m, E("file-read-to-string!", mt_keep(h))), E(text_read(f, SIZE_MAX))) && "the rest");
    fclose(f);
    require("file-close!", mt_one_truth(mt_eval(m, E("file-close!", h))));
    h = value_of(m, E("file-open!", T(notes), T("a")));
    require("file-write!", mt_one_truth(mt_eval(m, E("file-write!", mt_keep(h), T("delta\n")))));
    require("file-close!", mt_one_truth(mt_eval(m, E("file-close!", h))));
    strcat(model, "delta\n");
    assert(answers_are(mt_eval(m, E("file-lines!", T(notes))), E(lines_of(model, 0))) && "an append handle adds a line");
    assert(answers_are(mt_eval(m, E("stdin")), E((int64_t)fileno(stdin))) && "stdin");
    assert(answers_are(mt_eval(m, E("stdout")), E((int64_t)fileno(stdout))) && "stdout");
    assert(answers_are(mt_eval(m, E("stderr")), E((int64_t)fileno(stderr))) && "stderr");
    assert(answers_are(mt_eval(m, E("stderr!", T(""))), E(B(fputs("", stderr) != EOF))) && "stderr! writes");

    /* replace-file! publishes by rename: C's FILE* opened before it keeps
       the old file, while the path names the new one. */
    h = value_of(m, E("file-open!", T(notes), T("r")));
    FILE *old = fopen(notes, "r");
    require("C holds the old file", old != NULL);
    require("replace-file!", mt_one_truth(mt_eval(m, E("replace-file!", T(notes), T("fresh")))));
    snprintf(model, sizeof model, "fresh");
    assert(answers_are(mt_eval(m, E("file-read-exact!", mt_keep(h), 5)), E(text_read(old, 5))) && "a handle keeps the old file");
    fclose(old);
    require("file-close!", mt_one_truth(mt_eval(m, E("file-close!", h))));
    assert(answers_are(mt_eval(m, E("read-file!", T(notes))), E(T(model))) && "the path names the new one");
    static const unsigned char hi[] = { 104, 105 };
    assert(answers_are(mt_eval(m, E("replace-file!", T(blob), mt_array(2, hi))), E(B(true))) && "replacing with octets");
    assert(answers_are(mt_eval(m, E("read-file!", T(blob))), E(mt_textn((const char *)hi, 2))) && "which read as text");
    char copied[PATH_MAX], absent[PATH_MAX];
    snprintf(copied, sizeof copied, "%s", under(dir, "blob-copy.bin"));
    snprintf(absent, sizeof absent, "%s", under(dir, "absent.bin"));
    assert(answers_are(mt_eval(m, E("copy-file!", T(blob), T(copied))), E(B(true))) && "copy-file!");
    assert(answers_are(mt_eval(m, E("read-bytes!", T(copied))), E(mt_array(2, hi))) && "a copy has the same octets");
    assert(answers_are(mt_eval(m, guarded(E("copy-file!", T(absent), T(copied)))), E(verdict(access(absent, F_OK) == 0)))
           && "a copy needs a source");
    assert(answers_are(mt_eval(m, E("read-bytes!", T(copied))), E(mt_array(2, hi))) && "and a failed copy replaces nothing");

    /* Directories, trees and links. */
    char tree[PATH_MAX];
    snprintf(tree, sizeof tree, "%s", under(dir, "tree"));
    assert(answers_are(mt_eval(m, E("make-dir!", T(under(tree, "deep/deeper")))), E(B(true))) && "make-dir! makes the parents too");
    assert(answers_are(mt_eval(m, E("write-file!", T(under(tree, "deep/deeper/leaf.txt")), T("leaf"))), E(B(true))) && "a leaf");
    assert(answers_are(mt_eval(m, E("write-file!", T(under(tree, "deep/note.md")), T("note"))), E(B(true))) && "a note");
    assert(answers_are(mt_eval(m, E("write-file!", T(under(tree, ".hidden")), T("h"))), E(B(true))) && "a dotfile");
    assert(answers_are(mt_eval(m, E("make-link!", T("deep"), T(under(tree, "shortcut")))), E(B(true))) && "a link down");
    assert(answers_are(mt_eval(m, E("make-link!", T(".."), T(under(tree, "deep/up")))), E(B(true))) && "a link up");
    assert(answers_are(mt_eval(m, E("file-kind", T(under(tree, "shortcut")))), E(kind(under(tree, "shortcut")))) && "a link's kind");
    assert(answers_are(mt_eval(m, E("read-link", T(under(tree, "shortcut")))), E(link_text(under(tree, "shortcut")))) && "read-link");
    assert(answers_are(mt_eval(m, E("same-file", T(under(tree, "shortcut")), T(under(tree, "deep")))), E(B(same_file(under(tree, "shortcut"), under(tree, "deep")))))
           && "same-file through a link");
    assert(answers_are(mt_eval(m, E("same-file", T(under(tree, "deep")), T(under(tree, "deep/note.md")))), E(B(same_file(under(tree, "deep"), under(tree, "deep/note.md")))))
           && "and not a file inside it");
    assert(answers_are(mt_eval(m, E("list-dir!", T(tree))), E(listed(tree))) && "list-dir!");
    assert(answers_are(mt_eval(m, collapsed(E("dir-walk", T(tree)))), E(walked(tree, false))) && "a walk reports links and stays out");
    assert(answers_are(mt_eval(m, collapsed(E("dir-walk", T(tree), E("quote", E(E("follow-links", B(true))))))), E(walked(tree, true)))
           && "following them, the link back up is reported and not entered");

    /* Globs. */
    mt_atom *hidden = E("quote", E(E("hidden", B(true)))), *follow = E("quote", E(E("follow-links", B(true))));
    assert(answers_are(mt_eval(m, collapsed(E("dir-glob", T(tree), T("deep/*.md")))), E(globbed(tree, "deep/*.md", false, false)))
           && "a wildcard in one directory");
    assert(answers_are(mt_eval(m, collapsed(E("dir-glob", T(tree), T("**/*.txt")))), E(globbed(tree, "**/*.txt", false, false)))
           && "** at any depth");
    assert(answers_are(mt_eval(m, collapsed(E("dir-glob", T(tree), T("*")))), E(globbed(tree, "*", false, false))) && "dotfiles stay out");
    assert(answers_are(mt_eval(m, collapsed(E("dir-glob", T(tree), T("*"), mt_keep(hidden)))), E(globbed(tree, "*", true, false)))
           && "unless asked for");
    assert(answers_are(mt_eval(m, collapsed(E("dir-glob", T(tree), T("{deep,other}/note.m?")))), E(globbed(tree, "{deep,other}/note.m?", false, false)))
           && "braces and ?");
    assert(answers_are(mt_eval(m, collapsed(E("dir-glob", T(tree), T("**/*.md"), mt_keep(follow)))), E(globbed(tree, "**/*.md", false, true)))
           && "** following links");
    assert(answers_are(mt_eval(m, collapsed(E("dir-glob", T(tree), T("nowhere/*")))), E(globbed(tree, "nowhere/*", false, false)))
           && "a literal component that is not there");
    mt_atom *unclosed = globbed(tree, "[unclosed", false, false);
    assert(answers_are(mt_eval(m, guarded(collapsed(E("dir-glob", T(tree), T("[unclosed"))))), E(verdict(unclosed != NULL)))
           && "an unclosed bracket");
    mt_drop(unclosed);
    mt_drop(hidden);
    mt_drop(follow);

    /* copy-dir!, rename-file! and delete-tree!. */
    char copy[PATH_MAX];
    snprintf(copy, sizeof copy, "%s", under(dir, "copies/tree"));
    assert(answers_are(mt_eval(m, E("copy-dir!", T(tree), T(copy))), E(B(true))) && "copy-dir!");
    assert(answers_are(mt_eval(m, E("read-file!", T(under(copy, "deep/deeper/leaf.txt")))), E(T("leaf"))) && "a copied file");
    assert(answers_are(mt_eval(m, E("read-link", T(under(copy, "shortcut")))), E(link_text(under(copy, "shortcut"))))
           && "a copied link holds the same text");
    assert(answers_are(mt_eval(m, guarded(E("copy-dir!", T(tree), T(copy)))), E(verdict(access(copy, F_OK) != 0)))
           && "a copy needs a free destination");
    assert(answers_are(mt_eval(m, guarded(E("copy-dir!", T(tree), T(under(tree, "inside"))))), E(verdict(!inside(under(tree, "inside"), tree))))
           && "outside its source");
    assert(answers_are(mt_eval(m, E("rename-file!", T(under(copy, "deep/note.md")), T(under(copy, "deep/renamed.md")))), E(B(true)))
           && "rename-file!");
    assert(answers_are(mt_eval(m, E("file-kind", T(under(copy, "deep/note.md")))), E(kind(under(copy, "deep/note.md")))) && "the old name is gone");
    assert(answers_are(mt_eval(m, E("read-file!", T(under(copy, "deep/renamed.md")))), E(T("note"))) && "the new one reads the same");
    assert(answers_are(mt_eval(m, guarded(E("rename-file!", T(under(copy, "deep")), T(under(copy, "deep/deeper"))))), E(verdict(!inside(under(copy, "deep/deeper"), under(copy, "deep")))))
           && "a directory cannot move inside itself");
    assert(answers_are(mt_eval(m, E("delete-tree!", T(under(copy, "shortcut")))), E(B(true))) && "deleting a link root");
    assert(answers_are(mt_eval(m, E("dir-exists", T(under(copy, "deep")))), E(B(is_dir(under(copy, "deep"))))) && "leaves its target");
    assert(answers_are(mt_eval(m, E("delete-tree!", T(copy))), E(B(true))) && "delete-tree!");
    assert(answers_are(mt_eval(m, E("dir-exists", T(copy))), E(B(is_dir(copy)))) && "and the tree is gone");
    assert(answers_are(mt_eval(m, E("delete-dir!", T(under(dir, "copies")))), E(B(true))) && "delete-dir!");

    /* Lexical paths never touch the filesystem; path-resolve does. */
    assert(answers_are(mt_eval(m, E("path-join", T("reports"), T("sales.csv"))), E(T(under("reports", "sales.csv")))) && "path-join");
    assert(answers_are(mt_eval(m, E("path-parent", T("reports/sales.csv"))), E(parent("reports/sales.csv"))) && "path-parent");
    assert(answers_are(mt_eval(m, E("path-name", T("reports/sales.csv"))), E(name_of("reports/sales.csv"))) && "path-name");
    assert(answers_are(mt_eval(m, E("path-extension", T("reports/sales.tar.gz"))), E(extension("reports/sales.tar.gz", false))) && "path-extension");
    assert(answers_are(mt_eval(m, E("path-stem", T("reports/sales.tar.gz"))), E(extension("reports/sales.tar.gz", true))) && "path-stem");
    assert(answers_are(mt_eval(m, E("path-parts", T("/reports/2026/sales.csv"))), E(path_parts("/reports/2026/sales.csv"))) && "path-parts");
    assert(answers_are(mt_eval(m, E("path-normalize", T("reports/./2026/../sales.csv"))), E(normalized("reports/./2026/../sales.csv")))
           && "path-normalize");
    assert(answers_are(mt_eval(m, E("path-normalize", T("/x/../../y"))), E(normalized("/x/../../y"))) && "no .. above the root");
    assert(answers_are(mt_eval(m, E("path-relative", T("/a/b/c"), T("/a/d"))), E(relative("/a/b/c", "/a/d"))) && "path-relative");
    assert(answers_are(mt_eval(m, E("path-absolute", T("/x/./y"))), E(absolute_of("/x/./y"))) && "path-absolute");
    assert(answers_are(mt_eval(m, E("path-parent", E("path-absolute", T("rel")))), E(absolute_of(".")))
           && "a relative path starts at the working directory");
    assert(answers_are(mt_eval(m, E("path-resolve", T(under(tree, "shortcut/deeper/leaf.txt")))), E(resolved(under(tree, "shortcut/deeper/leaf.txt"))))
           && "path-resolve follows a link down");
    assert(answers_are(mt_eval(m, E("path-resolve", T(under(tree, "deep/up/.hidden")))), E(resolved(under(tree, "deep/up/.hidden")))) && "and one up");

    /* Metadata, and kinds of what is not there. */
    mt_atom *meta = value_of(m, E("file-metadata!", T(notes)));
    assert(answers_are(mt_eval(m, E("match", mt_keep(meta), E("size", V("bytes")), V("bytes"))), E(size_of(notes))) && "a size in the metadata");
    mt_drop(meta);
    assert(answers_are(mt_eval(m, E("file-kind", T(under(dir, "nothing")))), E(kind(under(dir, "nothing")))) && "a missing path");
    char target[8];
    assert(answers_are(mt_eval(m, guarded(E("read-link", T(notes)))), E(verdict(readlink(notes, target, sizeof target) >= 0)))
           && "only a link has a target");

    /* Scopes apply a function to a resource and close it on every exit. */
    assert(answers_are(mt_eval(m, E("with-file", T(notes), T("r"), "file-read-to-string!")), E(T(model))) && "a scope applies a function's name");
    f = fopen(notes, "r");
    require("C opens the notes", f != NULL);
    assert(answers_are(mt_eval(m, E("with-file", T(notes), T("r"), "size-then-two")), E(size_of(notes), text_read(f, 2)))
           && "every answer streams while the handle is open");
    fclose(f);
    assert(answers_are(mt_eval(m, E("with-file", T(notes), T("r"), E("|->", E(V("h")), E("file-get-size!", V("h"))))), E(size_of(notes)))
           && "a scope applies a lambda");
    char scope[PATH_MAX];
    snprintf(scope, sizeof scope, "%s", under(dir, "file-lib-scope-XXXXXX"));
    require("C mints a scratch directory of its own", mkdtemp(scope) != NULL);
    FILE *scratch = fopen(under(scope, "scratch"), "w");
    require("and fills it", scratch != NULL && fputs("scratch", scratch) != EOF && fclose(scratch) == 0);
    assert(answers_are(mt_eval(m, E("with-temp-dir", T("file-lib-scope"), E("fill-and-list", T("scratch")))), E(listed(scope)))
           && "a scope applies a partial application");
    require("C clears its own", remove(under(scope, "scratch")) == 0 && rmdir(scope) == 0);
    bool open = false;
    assert(answers_are(mt_eval(m, guarded(E("with-file", T(notes), T("r"), "close-then-read"))), E(verdict(open)))
           && "a closed handle refuses, and the scope still closes");

    /* temp-path! mints a file the caller owns; a prefix names, never places. */
    mt_atom *fresh = value_of(m, E("temp-path!", T("file-lib")));
    assert(answers_are(mt_eval(m, E("file-exists", mt_keep(fresh))), E(B(is_file(mt_name(fresh))))) && "temp-path! makes the file");
    assert(answers_are(mt_eval(m, E("delete-file!", fresh)), E(B(true))) && "which the caller deletes");
    const char *placed = "logs/run";
    assert(answers_are(mt_eval(m, guarded(E("temp-path!", T(placed)))), E(verdict(!strchr(placed, '/')))) && "a prefix holds no separator");

    assert(answers_are(mt_eval(m, E("delete-tree!", T(dir))), E(B(true))) && "everything goes with the directory");
    assert(answers_are(mt_eval(m, E("dir-exists", T(dir))), E(B(is_dir(dir)))) && "which is gone");
    mt_close(m);
    return 0;
}
