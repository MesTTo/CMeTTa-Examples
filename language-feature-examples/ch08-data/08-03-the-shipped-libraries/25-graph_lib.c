/* Purpose: lib_graph, held against directed graphs in C: a vertex array in
 *   the standard order of terms, which mt_compare gives C, each vertex once,
 *   and a boolean adjacency matrix over it. A graph is determined by its
 *   vertices and edges, so every edit is those two lists changed and the
 *   graph rebuilt canonically, every endpoint made a vertex. The closure is
 *   Warshall's algorithm, which is the library's own fold over intermediate
 *   vertices; reachability is a closure row plus the vertex itself; acyclic
 *   is a closure with an empty diagonal; and a topological order is Kahn's
 *   algorithm by layers, each layer's roots in vertex order, refusing when a
 *   layer has no root. Vertices compare as terms, so a variable is a vertex
 *   only of a graph that holds that variable.
 * Guarantees: all fifty-two claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

enum { MOST = 16 };

/* The vertices borrow from atoms C holds for the whole run. */
typedef struct graph {
    const mt_atom *v[MOST];
    size_t n;
    bool adj[MOST][MOST];
} graph;

static int index_of(const graph *g, const mt_atom *x)
{
    for (size_t i = 0; i < g->n; i++)
        if (mt_compare(g->v[i], x) == 0) return (int)i;
    return -1;
}

/* The canonical graph of a vertex list and an edge list. */
static graph canonical(const mt_atom *const *vs, size_t nv, const mt_atom *const *from, const mt_atom *const *to, size_t ne)
{
    graph g = { .n = 0 };
    const mt_atom *all[3 * MOST];
    size_t n = 0;
    for (size_t i = 0; i < nv; i++) all[n++] = vs[i];
    for (size_t e = 0; e < ne; e++) all[n++] = from[e], all[n++] = to[e];
    qsort(all, n, sizeof *all, mt_order);
    for (size_t i = 0; i < n; i++)
        if (!g.n || mt_compare(g.v[g.n - 1], all[i]) != 0) {
            require("room for the vertices", g.n < MOST);
            g.v[g.n++] = all[i];
        }
    for (size_t e = 0; e < ne; e++) g.adj[index_of(&g, from[e])][index_of(&g, to[e])] = true;
    return g;
}

/* A graph's own lists, to change and rebuild. */
typedef struct lists {
    const mt_atom *v[2 * MOST], *from[MOST * MOST], *to[MOST * MOST];
    size_t nv, ne;
} lists;

static lists lists_of(const graph *g)
{
    lists l = { .nv = 0, .ne = 0 };
    for (size_t i = 0; i < g->n; i++) {
        l.v[l.nv++] = g->v[i];
        for (size_t j = 0; j < g->n; j++)
            if (g->adj[i][j]) l.from[l.ne] = g->v[i], l.to[l.ne++] = g->v[j];
    }
    return l;
}

static graph rebuilt(const lists *l) { return canonical(l->v, l->nv, l->from, l->to, l->ne); }

/* Edges from an expression of (From To) pairs; false when one is no pair. */
static bool read_edges(const mt_atom *edges, lists *l)
{
    for (size_t e = 0; e < mt_len(edges); e++) {
        const mt_atom *edge = mt_at(edges, e);
        if (mt_kind_of(edge) != MT_EXPR || mt_len(edge) != 2) return false;
        l->from[l->ne] = mt_at(edge, 0), l->to[l->ne++] = mt_at(edge, 1);
    }
    return true;
}

static graph graph_of(const mt_atom *vertices, const mt_atom *edges)
{
    lists l = { .nv = 0, .ne = 0 };
    for (size_t i = 0; i < mt_len(vertices); i++) l.v[l.nv++] = mt_at(vertices, i);
    require("edges are pairs", read_edges(edges, &l));
    return rebuilt(&l);
}

static mt_atom *kept(const mt_atom *const *at, size_t n)
{
    mt_atom *out[MOST];
    for (size_t i = 0; i < n; i++) out[i] = mt_keep(at[i]);
    return mt_exprv(n, out);
}

static mt_atom *row_of(const graph *g, size_t i)
{
    const mt_atom *ns[MOST];
    size_t n = 0;
    for (size_t j = 0; j < g->n; j++)
        if (g->adj[i][j]) ns[n++] = g->v[j];
    return kept(ns, n);
}

/* The library's shape: ((Vertex Neighbours) ...). */
static mt_atom *rows(const graph *g)
{
    mt_atom *out[MOST];
    for (size_t i = 0; i < g->n; i++) out[i] = E(mt_keep(g->v[i]), row_of(g, i));
    return mt_exprv(g->n, out);
}

static mt_atom *vertices(const graph *g) { return kept(g->v, g->n); }

static mt_atom *edges(const graph *g)
{
    lists l = lists_of(g);
    mt_atom *out[MOST * MOST];
    for (size_t e = 0; e < l.ne; e++) out[e] = E(mt_keep(l.from[e]), mt_keep(l.to[e]));
    return mt_exprv(l.ne, out);
}

/* NULL when the graph does not hold the vertex. */
static mt_atom *neighbours(const graph *g, const mt_atom *x)
{
    int i = index_of(g, x);
    return i < 0 ? NULL : row_of(g, (size_t)i);
}

/* Whether a value has the shape: pairs whose keys strictly increase, each
   neighbour list strictly increasing and naming only keys. */
static bool increasing(const mt_atom *x)
{
    if (mt_kind_of(x) != MT_EXPR) return false;
    for (size_t i = 1; i < mt_len(x); i++)
        if (mt_compare(mt_at(x, i - 1), mt_at(x, i)) >= 0) return false;
    return true;
}

static bool is_graph(const mt_atom *x)
{
    if (mt_kind_of(x) != MT_EXPR) return false;
    for (size_t i = 0; i < mt_len(x); i++) {
        const mt_atom *row = mt_at(x, i);
        if (mt_kind_of(row) != MT_EXPR || mt_len(row) != 2 || !increasing(mt_at(row, 1))) return false;
        if (i && mt_compare(mt_at(mt_at(x, i - 1), 0), mt_at(row, 0)) >= 0) return false;
    }
    for (size_t i = 0; i < mt_len(x); i++)
        for (size_t k = 0; k < mt_len(mt_at(mt_at(x, i), 1)); k++) {
            bool known = false;
            for (size_t j = 0; j < mt_len(x); j++) known |= mt_compare(mt_at(mt_at(x, j), 0), mt_at(mt_at(mt_at(x, i), 1), k)) == 0;
            if (!known) return false;
        }
    return true;
}

/* Edits: the lists changed, the graph rebuilt. */
static graph with_vertices(const graph *g, const mt_atom *vs)
{
    lists l = lists_of(g);
    for (size_t i = 0; i < mt_len(vs); i++) l.v[l.nv++] = mt_at(vs, i);
    return rebuilt(&l);
}

/* false when an edge is no pair. */
static bool with_edges(const graph *g, const mt_atom *es, graph *out)
{
    lists l = lists_of(g);
    if (!read_edges(es, &l)) return false;
    *out = rebuilt(&l);
    return true;
}

static bool listed(const mt_atom *xs, const mt_atom *x)
{
    for (size_t i = 0; i < mt_len(xs); i++)
        if (mt_compare(mt_at(xs, i), x) == 0) return true;
    return false;
}

static graph without_vertices(const graph *g, const mt_atom *vs)
{
    lists l = lists_of(g), kept_lists = { .nv = 0, .ne = 0 };
    for (size_t i = 0; i < l.nv; i++)
        if (!listed(vs, l.v[i])) kept_lists.v[kept_lists.nv++] = l.v[i];
    for (size_t e = 0; e < l.ne; e++)
        if (!listed(vs, l.from[e]) && !listed(vs, l.to[e])) kept_lists.from[kept_lists.ne] = l.from[e], kept_lists.to[kept_lists.ne++] = l.to[e];
    return rebuilt(&kept_lists);
}

static graph without_edges(const graph *g, const mt_atom *es)
{
    graph out = *g;
    for (size_t e = 0; e < mt_len(es); e++) {
        int i = index_of(g, mt_at(mt_at(es, e), 0)), j = index_of(g, mt_at(mt_at(es, e), 1));
        if (i >= 0 && j >= 0) out.adj[i][j] = false;
    }
    return out;
}

static graph transposed(const graph *g)
{
    graph out = *g;
    for (size_t i = 0; i < g->n; i++)
        for (size_t j = 0; j < g->n; j++) out.adj[i][j] = g->adj[j][i];
    return out;
}

static graph united(const graph *const *gs, size_t count)
{
    lists all = { .nv = 0, .ne = 0 };
    for (size_t k = 0; k < count; k++) {
        lists l = lists_of(gs[k]);
        for (size_t i = 0; i < l.nv; i++) all.v[all.nv++] = l.v[i];
        for (size_t e = 0; e < l.ne; e++) all.from[all.ne] = l.from[e], all.to[all.ne++] = l.to[e];
    }
    return rebuilt(&all);
}

/* Warshall: an edge i -> j once a path i -> k -> j exists, k taken in turn.
   Time: n^3 steps, n = vertices. */
static graph closed(const graph *g)
{
    graph out = *g;
    for (size_t k = 0; k < g->n; k++)
        for (size_t i = 0; i < g->n; i++)
            for (size_t j = 0; j < g->n; j++) out.adj[i][j] |= out.adj[i][k] && out.adj[k][j];
    return out;
}

static mt_atom *reachable(const graph *g, const mt_atom *x)
{
    graph c = closed(g);
    int i = index_of(&c, x);
    require("the origin is a vertex", i >= 0);
    c.adj[i][i] = true;
    return row_of(&c, (size_t)i);
}

static bool acyclic(const graph *g)
{
    graph c = closed(g);
    for (size_t i = 0; i < c.n; i++)
        if (c.adj[i][i]) return false;
    return true;
}

/* Kahn by layers: each layer is every vertex left with no edge into it from
   one left, in vertex order. NULL when a layer is empty, which a cycle
   makes. Time: n^2 per layer, at most n layers. */
static mt_atom *topological(const graph *g)
{
    bool gone[MOST] = { false };
    const mt_atom *order[MOST];
    size_t n = 0;
    while (n < g->n) {
        bool root[MOST];
        size_t before = n;
        for (size_t i = 0; i < g->n; i++) {
            root[i] = !gone[i];
            for (size_t j = 0; j < g->n && root[i]; j++) root[i] = gone[j] || !g->adj[j][i];
            if (root[i]) order[n++] = g->v[i];
        }
        if (n == before) return NULL;
        for (size_t i = 0; i < g->n; i++) gone[i] |= root[i];
    }
    return kept(order, n);
}

static mt_atom *verdict(bool holds) { return S(holds ? "fine" : "refused"); }
static mt_atom *guarded(mt_atom *goal) { return E("if-error", E("catch", goal), "refused", "fine"); }

static bool computed(mt_atom *value)
{
    mt_drop(value);
    return value != NULL;
}

static mt_atom *value_of(metta *m, mt_atom *goal)
{
    mt_atom *v = mt_one(mt_eval(m, goal));
    require("a value", v != NULL);
    return v;
}

/* The function an equation of `head` spells, read back from &self. */
static mt_atom *recipe(metta *m, mt_atom *head, const char *const *params, size_t count)
{
    mt_atom *lambda = NULL;
    mt_rows (row, mt_match(m, E("=", head, V("body")))) {
        mt_atom *names[2];
        for (size_t i = 0; i < count; i++) names[i] = mt_keep(mt_bound(row, params[i]));
        mt_drop(lambda);
        lambda = E("|->", mt_exprv(count, names), mt_keep(mt_bound(row, "body")));
    }
    require("the equation is in &self", lambda != NULL);
    return value_of(m, lambda);
}

int main(void)
{
    metta *m = open_engine();
    require("import lib_graph", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_graph")))));

    /* A graph from its edges, isolated vertices beside them. */
    mt_atom *none = mt_unit(), *lunch = E("lunch"), *chores = E(E("wake", "shower"), E("shower", "dress"), E("wake", "coffee")), *just_a = E("a");
    graph t = graph_of(lunch, chores), empty = graph_of(none, none), lone = graph_of(just_a, none);
    mt_atom *tasks = value_of(m, E("graph-of", mt_keep(lunch), mt_keep(chores)));
    check_atom("graph-of", mt_keep(tasks), rows(&t));
    check_answers("the empty graph", mt_eval(m, E("graph-of", mt_unit(), mt_unit())), rows(&empty));
    check_answers("an isolated vertex", mt_eval(m, E("graph-of", mt_keep(just_a), mt_unit())), rows(&lone));

    mt_atom *dangling = E(E("a", E("b"))), *unsorted = E(E("b", mt_unit()), E("a", E("b"))), *two = E(E("a", mt_unit()), E("b", mt_unit())),
            *seven = mt_num(7);
    check_answers("graph-is", mt_eval(m, E("graph-is", mt_keep(tasks))), B(is_graph(tasks)));
    check_answers("a neighbour that is no vertex", mt_eval(m, E("graph-is", mt_keep(dangling))), B(is_graph(dangling)));
    check_answers("rows out of order", mt_eval(m, E("graph-is", mt_keep(unsorted))), B(is_graph(unsorted)));
    check_answers("two isolated vertices", mt_eval(m, E("graph-is", mt_keep(two))), B(is_graph(two)));
    check_answers("a number", mt_eval(m, E("graph-is", mt_keep(seven))), B(is_graph(seven)));

    check_answers("graph-vertices", mt_eval(m, E("graph-vertices", mt_keep(tasks))), vertices(&t));
    check_answers("graph-edges", mt_eval(m, E("graph-edges", mt_keep(tasks))), edges(&t));
    check_answers("no edges", mt_eval(m, E("graph-edges", E("graph-of", mt_unit(), mt_unit()))), edges(&empty));

    mt_atom *wake = S("wake"), *lunch_v = S("lunch"), *dinner = S("dinner");
    check_answers("neighbours", mt_eval(m, E("graph-neighbours", mt_keep(tasks), mt_keep(wake))), neighbours(&t, wake));
    check_answers("a sink", mt_eval(m, E("graph-neighbours", mt_keep(tasks), mt_keep(lunch_v))), neighbours(&t, lunch_v));
    check_answers("an unknown vertex is refused", mt_eval(m, guarded(E("graph-neighbours", mt_keep(tasks), mt_keep(dinner)))),
                  verdict(computed(neighbours(&t, dinner))));

    /* Edits answer new graphs. */
    mt_atom *nap = E("nap"), *ab = E(E("a", "b")), *shower = E("shower"), *wake_coffee = E(E("wake", "coffee"));
    graph added, napped = with_vertices(&t, nap), fewer = without_vertices(&t, shower), cut = without_edges(&t, wake_coffee);
    require("C adds an edge", with_edges(&empty, ab, &added));
    check_answers("graph-add-vertices", mt_eval(m, E("graph-add-vertices", mt_keep(tasks), mt_keep(nap))), rows(&napped));
    check_answers("graph-add-edges", mt_eval(m, E("graph-add-edges", E("graph-of", mt_unit(), mt_unit()), mt_keep(ab))), rows(&added));
    check_answers("graph-remove-vertices", mt_eval(m, E("graph-remove-vertices", mt_keep(tasks), mt_keep(shower))), rows(&fewer));
    check_answers("graph-remove-edges", mt_eval(m, E("graph-remove-edges", mt_keep(tasks), mt_keep(wake_coffee))), rows(&cut));
    check_answers("the input stays", mt_eval(m, E("graph-vertices", mt_keep(tasks))), vertices(&t));

    /* Transpose, union, closure, reachability. */
    graph back = transposed(&t), closure = closed(&t);
    const graph *both[] = { &t, &back };
    graph undirected = united(both, 2);
    check_answers("the transpose reverses every edge", mt_eval(m, E("graph-edges", E("graph-transpose", mt_keep(tasks)))), edges(&back));
    check_answers("and keeps every vertex", mt_eval(m, E("graph-vertices", E("graph-transpose", mt_keep(tasks)))), vertices(&back));
    check_answers("a graph and its transpose", mt_eval(m, E("graph-edges", E("graph-union", mt_keep(tasks), E("graph-transpose", mt_keep(tasks))))),
                  edges(&undirected));
    check_answers("the closure", mt_eval(m, E("graph-edges", E("graph-closure", mt_keep(tasks)))), edges(&closure));
    check_answers("reachable", mt_eval(m, E("graph-reachable", mt_keep(tasks), mt_keep(wake))), reachable(&t, wake));
    check_answers("a vertex reaches itself", mt_eval(m, E("graph-reachable", mt_keep(tasks), mt_keep(lunch_v))), reachable(&t, lunch_v));

    /* Topological order, and the cycle that has none. */
    mt_atom *ring = E(E("a", "b"), E("b", "c"), E("c", "a")), *self = E(E("a", "a")), *a = S("a");
    graph cycle = graph_of(none, ring), selfish = graph_of(none, self);
    mt_atom *loop = value_of(m, E("graph-of", mt_unit(), mt_keep(ring)));
    check_answers("a topological order", mt_eval(m, E("graph-topological-order", mt_keep(tasks))), topological(&t));
    check_answers("acyclic", mt_eval(m, E("graph-is-acyclic", mt_keep(tasks))), B(acyclic(&t)));
    check_answers("a cycle", mt_eval(m, E("graph-is-acyclic", mt_keep(loop))), B(acyclic(&cycle)));
    check_answers("a cycle has no order", mt_eval(m, guarded(E("graph-topological-order", mt_keep(loop)))), verdict(computed(topological(&cycle))));
    check_answers("a self-edge is a cycle", mt_eval(m, E("graph-is-acyclic", E("graph-of", mt_unit(), mt_keep(self)))), B(acyclic(&selfish)));
    check_answers("around the cycle", mt_eval(m, E("graph-reachable", mt_keep(loop), mt_keep(a))), reachable(&cycle, a));

    /* Every head refuses what is no graph. */
    mt_atom *short_edge = E(E("a"));
    graph unused;
    check_answers("vertices of no graph", mt_eval(m, guarded(E("graph-vertices", mt_keep(dangling)))), verdict(is_graph(dangling)));
    check_answers("a closure of a number", mt_eval(m, guarded(E("graph-closure", mt_keep(seven)))), verdict(is_graph(seven)));
    check_answers("an edge that is no pair", mt_eval(m, guarded(E("graph-add-edges", mt_keep(tasks), mt_keep(short_edge)))),
                  verdict(with_edges(&t, short_edge, &unused)));

    /* Union of any number of graphs. */
    mt_atom *nap_only = E("nap");
    graph napper = graph_of(nap_only, none);
    const graph *three[] = { &t, &cycle, &napper }, *pair[] = { &t, &cycle }, *just_t[] = { &t };
    graph none_united = united(NULL, 0), t_united = united(just_t, 1), all_three = united(three, 3), both_graphs = united(pair, 2);
    check_answers("the union of none", mt_eval(m, E("graph-union")), rows(&none_united));
    check_answers("the union of one", mt_eval(m, E("graph-union", mt_keep(tasks))), rows(&t_united));
    check_answers("the union of three",
                  mt_eval(m, E("graph-vertices", E("graph-union", mt_keep(tasks), mt_keep(loop), E("graph-of", mt_keep(nap_only), mt_unit())))),
                  vertices(&all_three));
    check_answers("a runtime collection",
                  mt_eval(m, E("graph-vertices", E("apply-to", "graph-union", E("quote", E(mt_keep(tasks), mt_keep(loop)))))), vertices(&both_graphs));
    mt_atom *to_b = E(E("a", "b")), *to_c = E(E("a", "c"));
    graph gb = graph_of(none, to_b), gc = graph_of(none, to_c);
    check_answers("a graph per alternative",
                  mt_eval(m, E("collapse", E("graph-neighbours", E("graph-of", mt_unit(), E("superpose", E(mt_keep(to_b), mt_keep(to_c)))), mt_keep(a)))),
                  E(neighbours(&gb, a), neighbours(&gc, a)));

    /* Variables are vertices by sharing, never wildcards. */
    mt_atom *x = V("x"), *y = V("y"), *open = E(E("a", E(mt_keep(x))), E("b", mt_unit())), *shared = E(E(mt_keep(x), E(mt_keep(y))), E(mt_keep(y), mt_unit())),
            *missing = V("missing"), *xy = E(mt_keep(x), mt_keep(y)), *xy_edge = E(E(mt_keep(x), mt_keep(y)));
    graph g_xy = graph_of(xy, xy_edge);
    mt_atom *g_xy_rows = rows(&g_xy);
    check_answers("a variable neighbour is no vertex here", mt_eval(m, E("let", V("graph"), E("quote", mt_keep(open)), E("graph-is", V("graph")))),
                  B(is_graph(open)));
    check_answers("but is where it is shared", mt_eval(m, E("let", V("graph"), E("quote", mt_keep(shared)), E("graph-is", V("graph")))),
                  B(is_graph(shared)));
    check_answers("a variable looks up nothing", mt_eval(m, guarded(E("graph-neighbours", mt_keep(tasks), mt_keep(missing)))),
                  verdict(computed(neighbours(&t, missing))));
    check_answers("variables build a graph",
                  mt_eval(m, E("let", V("graph"), E("graph-of", E("quote", mt_keep(xy)), E("quote", mt_keep(xy_edge))),
                               E("==", V("graph"), E("quote", mt_keep(shared))))),
                  B(mt_eq(g_xy_rows, shared)));
    graph shared_closure = closed(&g_xy);
    mt_atom *shared_closed = rows(&shared_closure);
    check_answers("and close over it",
                  mt_eval(m, E("let", V("graph"), E("quote", mt_keep(shared)), E("==", E("graph-closure", V("graph")), V("graph")))),
                  B(mt_eq(shared_closed, shared)));

    /* Runnable data is a vertex like any other. */
    mt_atom *sum = E("+", 1, 2), *error = E("Error", "data", "code"), *data_edge = E(E(mt_keep(sum), mt_keep(error))),
            *error_loop = E(E(mt_keep(error), mt_keep(error))), *sum_only = E(mt_keep(sum)), *letters = E(E("Error", "a"), E("a", "b")),
            *error_v = S("Error");
    graph data = graph_of(none, data_edge), data_loop = graph_of(none, error_loop), spelled = graph_of(none, letters);
    graph data_fewer = without_vertices(&data, sum_only);
    check_answers("neighbours of an expression",
                  mt_eval(m, E("graph-neighbours", E("graph-of", mt_unit(), E("quote", mt_keep(data_edge))), E("quote", mt_keep(sum)))),
                  neighbours(&data, sum));
    check_answers("removing an expression",
                  mt_eval(m, E("graph-remove-vertices", E("graph-of", mt_unit(), E("quote", mt_keep(data_edge))), E("quote", mt_keep(sum_only)))),
                  rows(&data_fewer));
    check_answers("an Error vertex's self-edge",
                  mt_eval(m, E("graph-is-acyclic", E("graph-of", mt_unit(), E("quote", mt_keep(error_loop))))), B(acyclic(&data_loop)));
    check_answers("a vertex named Error",
                  mt_eval(m, E("graph-reachable", E("graph-of", mt_unit(), E("quote", mt_keep(letters))), mt_keep(error_v))),
                  reachable(&spelled, error_v));

    /* The path recipe is an equation this space holds. */
    const char *graph_and_vertex[] = { "graph", "vertex" }, *vertex_only[] = { "vertex" };
    mt_atom *walk = recipe(m, E("graph-reachable", V("graph"), V("vertex")), graph_and_vertex, 2);
    check_answers("a recipe applied", mt_eval(m, E(walk, mt_keep(tasks), mt_keep(wake))), reachable(&t, wake));
    mt_atom *shower_v = S("shower"), *from_tasks = recipe(m, E("graph-reachable", mt_keep(tasks), V("vertex")), vertex_only, 1);
    check_answers("a recipe specialized to one graph", mt_eval(m, E(from_tasks, mt_keep(shower_v))), reachable(&t, shower_v));

    /* Layers in canonical order; a cycle's downstream vertex is no cycle. */
    mt_atom *alone = E("alone"), *spread = E(E("a", "z"), E("b", "c")), *tailed = E(E("a", "b"), E("b", "a"), E("b", "tail"));
    graph layered = graph_of(alone, spread), with_tail = graph_of(none, tailed);
    check_answers("isolated vertices lead", mt_eval(m, E("graph-topological-order", E("graph-of", mt_keep(alone), mt_keep(spread)))),
                  topological(&layered));
    check_answers("a cycle with a tail", mt_eval(m, E("graph-is-acyclic", E("graph-of", mt_unit(), mt_keep(tailed)))), B(acyclic(&with_tail)));

    mt_atom *held[] = { none, lunch, chores, just_a, tasks, dangling, unsorted, two, seven, wake, lunch_v, dinner, nap, ab, shower,
                        wake_coffee, ring, self, a, loop, short_edge, nap_only, to_b, to_c, x, y, open, shared, missing, xy, xy_edge,
                        g_xy_rows, shared_closed, sum, error, data_edge, error_loop, sum_only, letters, error_v, walk, shower_v,
                        from_tasks, alone, spread, tailed };
    for (size_t i = 0; i < sizeof held / sizeof *held; i++) mt_drop(held[i]);
    return done(m);
}
