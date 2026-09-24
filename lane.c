/* Purpose: describe &self the way the twin lane compares it: which heads its
 *   equations define, which C operations are published, and the multiset of
 *   atoms it holds, each written canonically.
 * Guarantees:
 *   - a canonical line renumbers variables by first occurrence, so atoms equal
 *     up to renaming print alike in every process, and an anonymous `_` is a
 *     new variable at each occurrence, as the engine reads one
 *     [tested: make twins; commit=WORKTREE]
 *   - LANE-HASH is the wrapping sum of each line's FNV-1a 64 hash, a multiset
 *     hash, so enumeration order cannot move it and a duplicate atom does
 *     [source: Bellare and Micciancio, "A New Paradigm for Collision-free
 *     Hashing: Incrementality at Reduced Cost", EUROCRYPT 1997, AdHash]
 *   - LANE-DATA-HASH is the same sum over the atoms that are not equations,
 *     and each equation, (= head body), also prints as a LANE-EQUATION line,
 *     with LANE-EQUATIONS counting them, so a space too large to list still
 *     shows the definitions a twin's C functions carry one by one
 *     [tested: make twins; commit=WORKTREE]
 * Decides: LANE-ATOM lines stop at 50,000 atoms, the cap the Python lane uses
 *   for the same diagnostic, and LANE-EQUATION lines at 50,000 equations;
 *   the hashes and counts still cover every atom
 *   [source: extensions/python/tools/twin_coverage.py, CONTENT_CAP;
 *   commit=7d995f762ba535440834d6edd071679f672fdca9].
 * Fails when: an atom holds a live C object or a native handle; those print
 *   as their type and presentation, which says nothing across processes.
 */
#include "lane.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { ATOM_CAP = 50000 };

typedef struct text {
    char *data;
    size_t len, cap;
} text;

static void put(text *t, const char *bytes, size_t n)
{
    if (t->len + n + 1 > t->cap) {
        size_t cap = t->cap ? t->cap : 64;
        while (cap < t->len + n + 1) cap *= 2;
        char *grown = realloc(t->data, cap);
        if (!grown) { fputs("lane: out of memory\n", stderr); exit(EXIT_FAILURE); }
        t->data = grown;
        t->cap = cap;
    }
    memcpy(t->data + t->len, bytes, n);
    t->len += n;
    t->data[t->len] = '\0';
}

static void puts_text(text *t, const char *s) { put(t, s, strlen(s)); }

/* The canonical spelling of one leaf. */
static void leaf(text *t, const mt_atom *a)
{
    switch (mt_kind_of(a)) {
    case MT_OBJECT:
        puts_text(t, "#object:");
        puts_text(t, mt_type(a) ? mt_type(a) : "?");
        return;
    case MT_HANDLE:
        puts_text(t, "#handle:");
        puts_text(t, mt_show(a));
        return;
    default: {
        mt_string w = mt_write_dup(a);
        if (w.data) {
            put(t, w.data, w.len);
            mt_free(w.data);
        } else {
            puts_text(t, "#shown:");
            puts_text(t, mt_show(a));
        }
    }
    }
}

typedef struct frame {
    const mt_atom *node;
    size_t next;
} frame;

/* One atom, written with its variables renumbered by first occurrence.
   Time and space: O(n + v^2) for n nodes and v distinct variables, the
   renumbering being a linear search the atoms of a space keep short. */
static void canonical(text *t, const mt_atom *root)
{
    const char **names = NULL;
    size_t nnames = 0, capnames = 0, anonymous = 0;
    frame *stack = NULL;
    size_t depth = 0, capdepth = 0;
    char number[32];

    t->len = 0;
    if (t->data) t->data[0] = '\0';
#define PUSH(n) do {                                                        \
        if (depth == capdepth) {                                            \
            capdepth = capdepth ? 2 * capdepth : 16;                        \
            stack = realloc(stack, capdepth * sizeof *stack);               \
            if (!stack) { fputs("lane: out of memory\n", stderr); exit(1); } \
        }                                                                   \
        stack[depth].node = (n); stack[depth].next = 0; depth++;            \
    } while (0)
    PUSH(root);
    while (depth) {
        frame *f = &stack[depth - 1];
        const mt_atom *a = f->node;
        if (mt_kind_of(a) != MT_EXPR) {
            if (mt_kind_of(a) == MT_VARIABLE) {
                const char *name = mt_name(a);
                size_t index = nnames;
                if (strcmp(name, "_") != 0) {
                    for (size_t i = 0; i < nnames; i++)
                        if (names[i] && strcmp(names[i], name) == 0) { index = i; break; }
                } else {
                    anonymous++;
                }
                if (index == nnames) {
                    if (nnames == capnames) {
                        capnames = capnames ? 2 * capnames : 8;
                        names = realloc(names, capnames * sizeof *names);
                        if (!names) { fputs("lane: out of memory\n", stderr); exit(1); }
                    }
                    names[nnames++] = strcmp(name, "_") == 0 ? NULL : name;
                }
                snprintf(number, sizeof number, "$_%zu", index);
                puts_text(t, number);
            } else {
                leaf(t, a);
            }
            depth--;
            if (depth) put(t, " ", 1);
            continue;
        }
        if (f->next == 0) put(t, "(", 1);
        if (f->next < mt_len(a)) {
            const mt_atom *child = mt_at(a, f->next++);
            PUSH(child);
            continue;
        }
        if (t->len && t->data[t->len - 1] == ' ') t->len--;
        put(t, ")", 1);
        depth--;
        if (depth) put(t, " ", 1);
    }
#undef PUSH
    (void)anonymous;
    free(stack);
    free(names);
}

static uint64_t fnv1a(const char *s, size_t n)
{
    uint64_t h = 14695981039346656037ULL;
    for (size_t i = 0; i < n; i++) {
        h ^= (unsigned char)s[i];
        h *= 1099511628211ULL;
    }
    return h;
}

static int by_text(const void *a, const void *b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

/* The key an equation's head defines, name/arity, as the Python lane keys it. */
static char *head_key(const mt_atom *head)
{
    text t = {0};
    char arity[32];
    if (mt_kind_of(head) == MT_EXPR && mt_len(head) > 0) {
        const mt_atom *name = mt_at(head, 0);
        puts_text(&t, mt_kind_of(name) == MT_SYMBOL ? mt_name(name) : mt_show(name));
        snprintf(arity, sizeof arity, "/%zu", mt_len(head) - 1);
    } else {
        puts_text(&t, mt_kind_of(head) == MT_SYMBOL ? mt_name(head) : mt_show(head));
        snprintf(arity, sizeof arity, "/0");
    }
    puts_text(&t, arity);
    return t.data;
}

/* An equation, (= head body): what a twin may carry in a C function instead. */
static bool equation(const mt_atom *a)
{
    return mt_kind_of(a) == MT_EXPR && mt_len(a) == 3 && mt_kind_of(mt_at(a, 0)) == MT_SYMBOL &&
           strcmp(mt_name(mt_at(a, 0)), "=") == 0;
}

static void print_sorted_unique(const char *marker, char **items, size_t n)
{
    qsort(items, n, sizeof *items, by_text);
    printf("%s", marker);
    for (size_t i = 0; i < n; i++)
        if (i == 0 || strcmp(items[i], items[i - 1]) != 0) printf(" %s", items[i]);
    putchar('\n');
    for (size_t i = 0; i < n; i++) free(items[i]);
    free(items);
}

void lane_report(metta *runtime)
{
    char **keys = NULL;
    size_t n = 0, cap = 0;

    mt_each (eq, mt_match(runtime, mt_expr("=", mt_var("head"), mt_var("body")))) {
        if (n == cap) {
            cap = cap ? 2 * cap : 16;
            keys = realloc(keys, cap * sizeof *keys);
            if (!keys) { fputs("lane: out of memory\n", stderr); exit(1); }
        }
        keys[n++] = head_key(mt_at(eq, 1));
    }
    print_sorted_unique("LANE-HEADS", keys, n);

    keys = NULL;
    n = cap = 0;
    for (size_t i = 0; mt_seam_at(runtime, "op", i); i++) {
        const mt_seam_row *row = mt_seam_at(runtime, "op", i);
        if (n == cap) {
            cap = cap ? 2 * cap : 16;
            keys = realloc(keys, cap * sizeof *keys);
            if (!keys) { fputs("lane: out of memory\n", stderr); exit(1); }
        }
        text name = {0};
        puts_text(&name, row->name);
        keys[n++] = name.data;
    }
    print_sorted_unique("LANE-OPS", keys, n);

    size_t held = mt_count(runtime), equations = 0;
    uint64_t hash = 0, data = 0;
    text line = {0};
    printf("LANE-HELD %zu\n", held);
    mt_each (atom, mt_atoms(runtime)) {
        canonical(&line, atom);
        uint64_t h = fnv1a(line.data, line.len);
        hash += h;
        if (!equation(atom))
            data += h;
        else if (++equations <= ATOM_CAP)
            printf("LANE-EQUATION %s\n", line.data);
        if (held <= ATOM_CAP) printf("LANE-ATOM %s\n", line.data);
    }
    free(line.data);
    printf("LANE-HASH %016" PRIx64 "\n", hash);
    printf("LANE-EQUATIONS %zu\n", equations);
    printf("LANE-DATA-HASH %016" PRIx64 "\n", data);
    mt_clear();
}
