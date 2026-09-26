/* Purpose: a table over a space stays fresh when the space changes. C keeps
 *   the edge relation in its own array and writes every change to &self as
 *   well; reach and twohop are the original's equations over the space,
 *   tabled, and after each write the engine's tabled answers must be the
 *   ones C computes from its array: the ys an edge from x reaches, and the
 *   zs two edges do. A tabled call answers from SWI's answer trie rather than
 *   in clause order, so both sides are compared sorted by mt_order. A read
 *   the engine cannot resolve to one stored relation is refused rather than
 *   tabled without the guarantee, and C compares the refusal's structure.
 * Guarantees: all six claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

typedef struct edge {
    const char *from, *to;
} edge;

static edge edges[8];
static size_t count;

static void add_edge(metta *m, const char *from, const char *to)
{
    edges[count++] = (edge){ from, to };
    require("add the edge to &self too", mt_add(m, E("edge", from, to)));
}

static void remove_edge(metta *m, const char *from, const char *to)
{
    for (size_t i = 0; i < count; i++)
        if (strcmp(edges[i].from, from) == 0 && strcmp(edges[i].to, to) == 0) edges[i--] = edges[--count];
    require("remove the edge from &self too", mt_del(m, E("edge", from, to)));
}

/* The symbols a list of names spells, sorted by mt_order. */
static mt_list symbols(const char **names, size_t n)
{
    mt_list list = { mt_alloc(n * sizeof *list.items), n };
    require("room for the expectation", n == 0 || list.items != NULL);
    for (size_t i = 0; i < n; i++) list.items[i] = S(names[i]);
    qsort(list.items, list.len, sizeof *list.items, mt_order);
    return list;
}

/* C's reach: the ys an edge from x arrives at. Time: count comparisons. */
static mt_list reach(const char *x)
{
    const char *found[8];
    size_t n = 0;
    for (size_t i = 0; i < count; i++)
        if (strcmp(edges[i].from, x) == 0) found[n++] = edges[i].to;
    return symbols(found, n);
}

/* C's twohop: the zs a pair of edges x -> y -> z arrives at, each pair once,
   as the conjunction answers them. Time: count^2 comparisons. */
static mt_list twohop(const char *x)
{
    const char *found[64];
    size_t n = 0;
    for (size_t i = 0; i < count; i++)
        for (size_t j = 0; j < count; j++)
            if (strcmp(edges[i].from, x) == 0 && strcmp(edges[j].from, edges[i].to) == 0) found[n++] = edges[j].to;
    return symbols(found, n);
}

/* The engine's answers for (f x $v), sorted by mt_order, held against C's. */
static void agree(metta *m, const char *claim, const char *f, const char *x, mt_list want)
{
    mt_list got = mt_all(mt_eval(m, E(f, x, V("v"))));
    qsort(got.items, got.len, sizeof *got.items, mt_order);
    assert(list_is(got, mt_exprv(want.len, want.items)) && claim);
    mt_free(want.items);
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_tabling", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_tabling")))));
    add_edge(m, "a", "b");
    add_edge(m, "b", "c");
    require("reach", mt_add(m, E("=", E("reach", V("x"), V("y")), E("match", "&self", E("edge", V("x"), V("y")), V("y")))));
    require("twohop", mt_add(m, E("=", E("twohop", V("x"), V("z")),
                                 E("match", "&self", E(",", E("edge", V("x"), V("y")), E("edge", V("y"), V("z"))), V("z")))));
    require("table reach", mt_one_truth(mt_eval(m, E("tabled", E("reach", V("x"), V("y"))))));
    require("table twohop", mt_one_truth(mt_eval(m, E("tabled", E("twohop", V("x"), V("z"))))));

    agree(m, "reach from a", "reach", "a", reach("a"));
    agree(m, "twohop from a", "twohop", "a", twohop("a"));
    add_edge(m, "a", "c");
    agree(m, "an added edge the table read reaches it", "reach", "a", reach("a"));
    remove_edge(m, "a", "b");
    agree(m, "and a removed one", "reach", "a", reach("a"));
    add_edge(m, "c", "d");
    agree(m, "a conjunction tracks both its patterns", "twohop", "b", twohop("b"));

    require("bypattern", mt_add(m, E("=", E("bypattern", V("p")), E("match", "&self", V("p"), V("p")))));
    assert(answers_are(mt_eval(m, E("catch", E("tabled", E("bypattern", V("p"))))), E(E("Error", E("metta_tabling_unresolved_read", "match", V("p")), "none")))
           && "a read no stored relation resolves is refused");
    mt_close(m);
    return 0;
}
