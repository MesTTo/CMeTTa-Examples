/* Purpose: a C graph algorithm over MeTTa facts. shortest-path is a C
 *   function the engine calls: it reads the (edge from to) facts, runs a
 *   breadth-first search in C arrays, and answers the path as an atom the
 *   program then stores as knowledge like any other fact.
 * Assumes: vertices are small nonnegative integers.
 * Owns resources: the callback owns its adjacency matrix, queue and parent
 *   array, freed on every path.
 * Guarantees: the unique shortest path 0-1-2-3 is found and queryable, and a
 *   route against the edges' direction answers nothing
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>

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

/* The same question as a truth value, for a condition with more in it:
   borrows the atom the program holds and takes the expectation. */
static inline bool alpha_equal(const mt_atom *got, mt_atom *want)
{
    bool equal = got && want && mt_alpha_eq(got, want);
    mt_drop(want);
    return equal;
}

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
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    static const int64_t edges[][2] = { {0, 1}, {1, 2}, {2, 3}, {0, 4}, {4, 5}, {5, 6}, {6, 3} };
    for (size_t i = 0; i < sizeof edges / sizeof edges[0]; i++)
        require("store an edge", mt_add(m, E("edge", edges[i][0], edges[i][1])));
    require("publish shortest-path", mt_def(m, (mt_op){ .name = "shortest-path", .arity = 2,
                                                        .effect = MT_EFFECT_CLASS_READ_ONLY_LOOKUP, .fn = shortest }));

    mt_atom *path = mt_one(mt_eval(m, E("shortest-path", 0, 3)));
    assert(alpha_equal(path, E("Path", 0, 1, 2, 3)) && "the unique shortest path is 0 1 2 3");
    require("store the path as knowledge", mt_add(m, path));
    assert(answers_are(mt_match(m, E("Path", V("a"), V("b"), V("c"), V("d"))), E(E("Path", 0, 1, 2, 3)))
           && "the computed path is now a fact");
    assert(!mt_first(mt_eval(m, E("shortest-path", 3, 0))) && mt_ok() && "no route runs against the edges");
    require("withdraw shortest-path", mt_undef(m, "shortest-path"));
    mt_close(m);
    return 0;
}
