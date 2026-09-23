/* Purpose: project edge atoms into a C graph and return a shortest path as data.
 * Owns resources: callback owns its adjacency matrix, queue and answer snapshot.
 * Assumes: vertices are nonnegative contiguous integer identifiers.
 * Guarantees: breadth-first search returns the unique shortest path and the
 *   result is queryable knowledge [tested: make check; commit=WORKTREE].
 * Open Obligations: None.
 */
#include "common.h"
/* Dense graph BFS: O(V^2+E) time and O(V^2) space, V vertices and E edges. */
static mt_status shortest(mt_call *call, void *user)
{
    (void)user; metta *m = mt_of(call);
    mt_list edges = mt_all(mt_match(m, mt_parse("(edge $from $to)")));
    if (!mt_ok()) return mt_error();
    size_t vertices = 0;
    for (size_t i = 0; i < edges.len; ++i) for (size_t j = 1; j <= 2; ++j) {
        int64_t v = mt_int(mt_at(edges.items[i], j));
        if (!mt_ok() || v < 0 || (uint64_t)v >= SIZE_MAX) {
            mt_list_free(edges); return mt_fail(call, "graph needs nonnegative vertex identifiers");
        }
        if ((size_t)v >= vertices) vertices = (size_t)v + 1;
    }
    int64_t from = mt_int(mt_arg(call, 0)), to = mt_int(mt_arg(call, 1));
    if (!mt_ok() || from < 0 || to < 0 || (uint64_t)from >= vertices || (uint64_t)to >= vertices ||
        vertices > SIZE_MAX / vertices || vertices > SIZE_MAX / sizeof(size_t)) {
        mt_list_free(edges); return mt_fail(call, "graph vertex or allocation is out of range");
    }
    bool *matrix = calloc(vertices * vertices, sizeof(*matrix));
    size_t *queue = malloc(vertices * sizeof(*queue)), *parent = malloc(vertices * sizeof(*parent));
    check("allocate C graph", matrix && queue && parent);
    for (size_t i = 0; i < vertices; ++i) parent[i] = SIZE_MAX;
    for (size_t i = 0; i < edges.len; ++i)
        matrix[(size_t)mt_int(mt_at(edges.items[i],1))*vertices+(size_t)mt_int(mt_at(edges.items[i],2))] = true;
    mt_list_free(edges);
    size_t begin = 0, end = 1; queue[0] = (size_t)from; parent[from] = (size_t)from;
    while (begin < end) {
        size_t v = queue[begin++];
        for (size_t w = 0; w < vertices; ++w) if (matrix[v*vertices+w] && parent[w] == SIZE_MAX) {
            parent[w] = v; queue[end++] = w;
        }
    }
    mt_atom *answer = NULL;
    if (parent[to] != SIZE_MAX) {
        size_t length = 1;
        for (size_t v = (size_t)to; v != (size_t)from; v = parent[v]) ++length;
        mt_atom **parts = mt_calloc(length+1, sizeof(*parts)); check("allocate path", parts != NULL);
        parts[0] = mt_sym("Path");
        size_t v = (size_t)to;
        for (size_t i = length; i > 0; --i) { parts[i] = mt_num((int64_t)v); v = parent[v]; }
        answer = mt_exprv(length+1, parts); mt_free(parts);
    }
    free(matrix); free(queue); free(parent);
    return answer ? mt_answer(call, answer) : MT_FAIL;
}
int main(void)
{
    metta *m = open_engine();
    check("graph facts", mt_do(m, "(edge 0 1) (edge 1 2) (edge 2 3) (edge 0 4) (edge 4 5) (edge 5 6) (edge 6 3)"));
    check("publish graph algorithm", mt_def(m, (mt_op){.name="shortest-path", .arity=2, .effect=MT_LOOKUP, .fn=shortest}));
    mt_atom *path = mt_one(mt_eval(m, mt_expr("shortest-path", 0, 3)));
    check_atom("unique shortest path", path, "(Path 0 1 2 3)");
    check("store computed path", mt_add(m, path));
    check_answers("algorithm result becomes knowledge", mt_match(m, mt_parse("(Path $a $b $c $d)")), "(Path 0 1 2 3)");
    check_answers("unreachable directed route", mt_eval(m, mt_expr("shortest-path", 3, 0)), "");
    check("withdraw graph algorithm", mt_undef(m, "shortest-path"));
    return done(m, "ecosystem_graph");
}
