/* Purpose: a tagged program evaluated to its fixpoint by the engine, under
 *   several carriers, over a graph with a cycle. The facts are C's table of
 *   tagged edges, turned into (fact tag (edge x y)) atoms, and the two path
 *   rules are the terms they are. C closes the same rules over the same
 *   table under each carrier with semirings.h: the strongest path under
 *   bool, the cheapest under tropical, each rule adding its own tag, the
 *   pairs a path reaches under set, and derivations under counting and prob
 *   on an acyclic copy. The exact probability, which the formula carrier
 *   reads back under prob, is C's sum over the possible worlds of the edges.
 * Guarantees: all six claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"
#include "semirings.h"

static const char *const nodes[] = { "a", "b", "c" };
enum { A, B_, C_, NODES, RULE_TAG = 1 };

static const tagged_edge cyclic[] = { { A, B_, 0.6 }, { B_, A, 0.5 }, { A, C_, 0.2 }, { B_, C_, 0.3 } };
static const tagged_edge acyclic[] = { { A, B_, 0.6 }, { B_, C_, 0.5 }, { A, C_, 0.2 } };
#define COUNT(table) (sizeof table / sizeof *table)

static void program(metta *m, mt_space *space, const tagged_edge *edges, size_t n)
{
    for (size_t i = 0; i < n; i++)
        require("a tagged fact", space ? mt_add(space, E("fact", edges[i].tag, E("edge", nodes[edges[i].from], nodes[edges[i].to])))
                                       : mt_add(m, E("fact", edges[i].tag, E("edge", nodes[edges[i].from], nodes[edges[i].to]))));
    mt_atom *rules[] = {
        E("rule", RULE_TAG, E("path", V("x"), V("y")), E("premises", E("edge", V("x"), V("y")))),
        E("rule", RULE_TAG, E("path", V("x"), V("z")), E("premises", E("edge", V("x"), V("y")), E("path", V("y"), V("z")))),
    };
    for (size_t i = 0; i < 2; i++) require("a rule", space ? mt_add(space, rules[i]) : mt_add(m, rules[i]));
}

static mt_answers *under(metta *m, const char *space, mt_atom *carrier, mt_atom *pattern)
{
    return mt_eval(m, E("match-under", mt_spaceref(space), carrier, pattern));
}

static mt_atom *path_a_c(void) { return E("path", "a", "c"); }

int main(void)
{
    metta *m = open_engine();
    program(m, NULL, cyclic, COUNT(cyclic));
    check_answers("bool is the strongest path", under(m, "&self", S("bool"), path_a_c()),
                  E(path_a_c(), closure(&viterbi, cyclic, COUNT(cyclic), NODES, RULE_TAG, A, C_)));
    check_answers("tropical the cheapest", under(m, "&self", S("tropical"), path_a_c()),
                  E(path_a_c(), closure(&tropical, cyclic, COUNT(cyclic), NODES, RULE_TAG, A, C_)));
    int64_t reached = 0;
    for (int y = 0; y < NODES; y++) reached += reachable(cyclic, COUNT(cyclic), ~0u, NODES, A, y);
    int64_t rows = 0;
    mt_each (row, under(m, "&self", S("set"), E("path", "a", V("y")))) rows++;
    check_int("set answers each pair a path reaches once", rows, reached);

    mt_atom *exact = mt_first(under(m, "&self", E("formula", "prob"), path_a_c()));
    require("an exact answer", exact && mt_kind_of(exact) == MT_EXPR && mt_len(exact) == 2);
    check("the formula carrier reads the exact probability",
          fabs(mt_float(mt_at(exact, 1)) - exact_probability(cyclic, COUNT(cyclic), NODES, A, C_)) < 0.000001);
    mt_drop(exact);

    mt_space *dag = mt_space_open(m, "&dag");
    require("open &dag", dag != NULL);
    program(m, dag, acyclic, COUNT(acyclic));
    check_answers("counting counts derivations", under(m, "&dag", S("counting"), path_a_c()),
                  E(path_a_c(), (int64_t)closure(&counting, acyclic, COUNT(acyclic), NODES, RULE_TAG, A, C_)));
    check_answers("prob sums their products", under(m, "&dag", S("prob"), path_a_c()),
                  E(path_a_c(), closure(&prob, acyclic, COUNT(acyclic), NODES, RULE_TAG, A, C_)));
    mt_space_close(dag);
    return done(m);
}
