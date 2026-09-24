/* Purpose: C's model of a tagged program's fixpoint: the carriers as
 *   semirings, and the closure of the two path rules over a table of tagged
 *   edges computed under one, the generic single-source shortest-distance
 *   iteration run to stability [source: Mohri, "Semiring Frameworks and
 *   Algorithms for Shortest-Distance Problems", J. Automata, Languages and
 *   Combinatorics 7(3), 2002, Figure 1's generic relaxation]. A carrier
 *   whose plus is idempotent converges on a graph with a cycle; the others
 *   are asked only of acyclic graphs, and the closure refuses to answer a
 *   table that has not settled after as many rounds as the graph has nodes.
 *   exact_probability() is the possible-worlds reading: every subset of the
 *   edges weighted by its facts' probabilities, summing the worlds where the
 *   path exists, which counts a shared edge once.
 * Assumes: the includer includes common.h first.
 * Guarantees: each value the engine's fixpoint answers for the same program
 *   under the same carrier [tested: make twins; commit=WORKTREE].
 */
#ifndef CH22_SEMIRINGS_H
#define CH22_SEMIRINGS_H
#include <math.h>

typedef struct semiring {
    double zero, one;
    double (*plus)(double, double);
    double (*times)(double, double);
    bool unit_sources; /* every source reads as one, which counts derivations */
} semiring;

static inline double sr_add(double a, double b) { return a + b; }
static inline double sr_mul(double a, double b) { return a * b; }

/* bool here is the strongest path, max of products; tropical the cheapest,
   min of sums; counting and prob sum over derivations. */
static const semiring viterbi = { 0, 1, fmax, sr_mul, false };
static const semiring tropical = { INFINITY, 0, fmin, sr_add, false };
static const semiring counting = { 0, 1, sr_add, sr_mul, true };
static const semiring prob = { 0, 1, sr_add, sr_mul, false };

typedef struct tagged_edge {
    int from, to;
    double tag;
} tagged_edge;

/* The tag of (path from to) under S: (rule r (path x y) (premises (edge x y)))
   and (rule r (path x z) (premises (edge x y) (path y z))) closed over EDGES
   on NODES nodes. Time: O(nodes^3 * edges) semiring operations at worst. */
static inline double closure(const semiring *s, const tagged_edge *edges, size_t n, int nodes, double rule, int from, int to)
{
    double *path = malloc((size_t)(nodes * nodes) * sizeof *path);
    require("room", path != NULL);
    for (int i = 0; i < nodes * nodes; i++) path[i] = s->zero;
    bool settled = false;
    for (int round = 0; round <= nodes && !settled; round++) {
        settled = true;
        for (int x = 0; x < nodes; x++)
            for (int y = 0; y < nodes; y++) {
                double v = s->zero;
                for (size_t e = 0; e < n; e++) {
                    if (edges[e].from != x) continue;
                    double tag = s->unit_sources ? s->one : edges[e].tag, r = s->unit_sources ? s->one : rule;
                    if (edges[e].to == y) v = s->plus(v, s->times(tag, r));
                    v = s->plus(v, s->times(s->times(tag, path[edges[e].to * nodes + y]), r));
                }
                if (v != path[x * nodes + y]) settled = false;
                path[x * nodes + y] = v;
            }
    }
    require("the closure settled", settled);
    double answer = path[from * nodes + to];
    free(path);
    return answer;
}

/* Whether TO is reachable from FROM over the edges PRESENT marks. */
static inline bool reachable(const tagged_edge *edges, size_t n, unsigned present, int nodes, int from, int to)
{
    unsigned seen = 1u << from, frontier = seen;
    while (frontier) {
        unsigned next = 0;
        for (size_t e = 0; e < n; e++)
            if ((present >> e & 1u) && (frontier >> edges[e].from & 1u) && !(seen >> edges[e].to & 1u))
                next |= 1u << edges[e].to;
        seen |= next;
        frontier = next;
    }
    (void)nodes;
    return seen >> to & 1u;
}

/* The probability that a path from FROM to TO exists, each edge present
   independently with its tag. Time: 2^edges worlds, each a reachability. */
static inline double exact_probability(const tagged_edge *edges, size_t n, int nodes, int from, int to)
{
    require("few enough edges to enumerate their worlds", n < 8 * sizeof(unsigned));
    double total = 0;
    for (unsigned world = 0; world < 1u << n; world++) {
        double weight = 1;
        for (size_t e = 0; e < n; e++) weight *= (world >> e & 1u) ? edges[e].tag : 1 - edges[e].tag;
        if (reachable(edges, n, world, nodes, from, to)) total += weight;
    }
    return total;
}
#endif
