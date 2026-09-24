/* Purpose: a C graph algorithm over MeTTa facts. shortest-path is a C
 *   function the engine calls: it reads the (edge from to) facts, runs a
 *   breadth-first search in C arrays, and answers the path as an atom the
 *   program then stores as knowledge like any other fact.
 * Assumes: vertices are small nonnegative integers.
 * Owns resources: the callback owns its adjacency matrix, queue and parent
 *   array, freed on every path.
 * Guarantees: the unique shortest path 0-1-2-3 is found and queryable, and a
 *   route against the edges' direction answers nothing [tested: make check;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

/* Breadth-first search on a dense matrix. Time O(V^2 + E), space O(V^2) for
   V vertices and E edges. */
static mt_status shortest(mt_call *call, void *user)
{
    (void)user;
    mt_list edges = mt_all(mt_match(mt_of(call), E("edge", V("from"), V("to"))));
    if (!mt_ok()) return mt_error();
    size_t vertices = 0;
    mt_clear();
    for (size_t i = 0; i < edges.len; i++)
        for (size_t end = 1; end <= 2; end++) {
            int64_t v = mt_int(mt_at(edges.items[i], end));
            if (v >= 0 && (size_t)v >= vertices) vertices = (size_t)v + 1;
        }
    int64_t from = mt_int(mt_arg(call, 0)), to = mt_int(mt_arg(call, 1));
    if (!mt_ok() || from < 0 || to < 0 || (size_t)from >= vertices || (size_t)to >= vertices) {
        mt_list_free(edges);
        return mt_fail(call, "shortest-path wants vertices the edges name");
    }
    bool *adjacent = calloc(vertices * vertices, sizeof *adjacent);
    size_t *queue = malloc(vertices * sizeof *queue);
    size_t *parent = malloc(vertices * sizeof *parent);
    if (!adjacent || !queue || !parent) {
        free(adjacent); free(queue); free(parent);
        mt_list_free(edges);
        return mt_fail(call, "out of memory for the graph");
    }
    for (size_t i = 0; i < edges.len; i++)
        adjacent[(size_t)mt_int(mt_at(edges.items[i], 1)) * vertices +
                 (size_t)mt_int(mt_at(edges.items[i], 2))] = true;
    mt_list_free(edges);

    for (size_t v = 0; v < vertices; v++) parent[v] = SIZE_MAX;
    size_t head = 0, tail = 0;
    queue[tail++] = (size_t)from;
    parent[from] = (size_t)from;
    while (head < tail) {
        size_t v = queue[head++];
        for (size_t w = 0; w < vertices; w++)
            if (adjacent[v * vertices + w] && parent[w] == SIZE_MAX) {
                parent[w] = v;
                queue[tail++] = w;
            }
    }

    mt_status status = MT_FAIL;           /* unreachable: no answer */
    if (parent[to] != SIZE_MAX) {
        size_t length = 1;
        for (size_t v = (size_t)to; v != (size_t)from; v = parent[v]) length++;
        mt_atom **steps = mt_calloc(length + 1, sizeof *steps);
        if (!steps) {
            status = mt_fail(call, "out of memory for the path");
        } else {
            steps[0] = S("Path");
            size_t v = (size_t)to;
            for (size_t i = length; i > 0; i--, v = parent[v]) steps[i] = N((int64_t)v);
            status = mt_answer(call, mt_exprv(length + 1, steps));
            mt_free(steps);
        }
    }
    free(adjacent); free(queue); free(parent);
    return status;
}

int main(void)
{
    metta *m = open_engine();
    static const int64_t edges[][2] = { {0, 1}, {1, 2}, {2, 3}, {0, 4}, {4, 5}, {5, 6}, {6, 3} };
    for (size_t i = 0; i < sizeof edges / sizeof edges[0]; i++)
        require("store an edge", mt_add(m, E("edge", edges[i][0], edges[i][1])));
    require("publish shortest-path", mt_def(m, (mt_op){ .name = "shortest-path", .arity = 2,
                                                        .effect = MT_EFFECT_CLASS_READ_ONLY_LOOKUP, .fn = shortest }));

    mt_atom *path = mt_one(mt_eval(m, E("shortest-path", 0, 3)));
    check("the unique shortest path is 0 1 2 3", mt_alpha_eq(path, E("Path", 0, 1, 2, 3)));
    require("store the path as knowledge", mt_add(m, path));
    check_answers("the computed path is now a fact",
                  mt_match(m, E("Path", V("a"), V("b"), V("c"), V("d"))), E("Path", 0, 1, 2, 3));
    check_none("no route runs against the edges", mt_eval(m, E("shortest-path", 3, 0)));
    require("withdraw shortest-path", mt_undef(m, "shortest-path"));
    return done(m);
}
