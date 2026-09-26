/* Purpose: a tagged program evaluated to its fixpoint by the engine, under
 *   several carriers, over a graph with a cycle. The facts are C's table of
 *   tagged edges, turned into (fact tag (edge x y)) atoms, and the two path
 *   rules are the terms they are. C closes the same rules over the same
 *   table under each carrier with semirings.h: the strongest path under
 *   bool, the cheapest under tropical, each rule adding its own tag, the
 *   pairs a path reaches under set, and derivations under counting and prob
 *   on an acyclic copy. The exact probability, which the formula carrier
 *   reads back under prob, is C's sum over the possible worlds of the edges.
 * Guarantees: all six claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 * Build: cc 07-tagged_fixpoint.c $(pkg-config --cflags --libs cmetta) -lm
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include "_fixtures/semirings.h"

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
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    program(m, NULL, cyclic, COUNT(cyclic));
    assert(answers_are(under(m, "&self", S("bool"), path_a_c()), E(E(path_a_c(), closure(&viterbi, cyclic, COUNT(cyclic), NODES, RULE_TAG, A, C_))))
           && "bool is the strongest path");
    assert(answers_are(under(m, "&self", S("tropical"), path_a_c()), E(E(path_a_c(), closure(&tropical, cyclic, COUNT(cyclic), NODES, RULE_TAG, A, C_))))
           && "tropical the cheapest");
    int64_t reached = 0;
    for (int y = 0; y < NODES; y++) reached += reachable(cyclic, COUNT(cyclic), ~0u, NODES, A, y);
    int64_t rows = 0;
    mt_each (row, under(m, "&self", S("set"), E("path", "a", V("y")))) rows++;
    assert(rows == reached && "set answers each pair a path reaches once");

    mt_atom *exact = mt_first(under(m, "&self", E("formula", "prob"), path_a_c()));
    require("an exact answer", exact && mt_kind_of(exact) == MT_EXPR && mt_len(exact) == 2);
    assert(fabs(mt_float(mt_at(exact, 1)) - exact_probability(cyclic, COUNT(cyclic), NODES, A, C_)) < 0.000001
           && "the formula carrier reads the exact probability");
    mt_drop(exact);

    mt_space *dag = mt_space_open(m, "&dag");
    require("open &dag", dag != NULL);
    program(m, dag, acyclic, COUNT(acyclic));
    assert(answers_are(under(m, "&dag", S("counting"), path_a_c()), E(E(path_a_c(), (int64_t)closure(&counting, acyclic, COUNT(acyclic), NODES, RULE_TAG, A, C_))))
           && "counting counts derivations");
    assert(answers_are(under(m, "&dag", S("prob"), path_a_c()), E(E(path_a_c(), closure(&prob, acyclic, COUNT(acyclic), NODES, RULE_TAG, A, C_))))
           && "prob sums their products");
    mt_space_close(dag);
    mt_close(m);
    return 0;
}
