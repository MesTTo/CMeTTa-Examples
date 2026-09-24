/* Purpose: a table over a space stays fresh when the space changes. C keeps
 *   the edge relation in its own array and writes every change to &self as
 *   well; reach and twohop are the original's equations over the space,
 *   tabled, and after each write the engine's tabled answers must be the
 *   ones C computes from its array: the ys an edge from x reaches, and the
 *   zs two edges do. A tabled call answers from SWI's answer trie rather than
 *   in clause order, so both sides are compared sorted by mt_order. A read
 *   the engine cannot resolve to one stored relation is refused rather than
 *   tabled without the guarantee, and C compares the refusal's structure.
 * Guarantees: all six claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

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
    check_list_(claim, got, want.len, want.items);
    mt_free(want.items);
}

int main(void)
{
    metta *m = open_engine();
    require("import lib_tabling", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_tabling")))));
    add_edge(m, "a", "b");
    add_edge(m, "b", "c");
    require("reach", mt_lower(m, (reach $x $y), (match &self (edge $x $y) $y)));
    require("twohop", mt_lower(m, (twohop $x $z), (match &self (, (edge $x $y) (edge $y $z)) $z)));
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

    require("bypattern", mt_lower(m, (bypattern $p), (match &self $p $p)));
    check_answers("a read no stored relation resolves is refused",
                  mt_eval(m, E("catch", E("tabled", E("bypattern", V("p"))))),
                  E("Error", E("metta_tabling_unresolved_read", "match", V("p")), "none"));
    return done(m);
}
