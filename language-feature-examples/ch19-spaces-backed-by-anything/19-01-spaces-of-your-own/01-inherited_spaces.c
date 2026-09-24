/* Purpose: a child space reads through its parent and writes only to itself.
 *   C keeps each space's facts in its own table as it writes them, and holds
 *   the engine's reads against that: the chain read answers the child's
 *   facts, then the parent's; the two-edge join links a parent edge to a
 *   child edge because each link reads the whole chain, which C computes by
 *   joining the union of the two tables; the child's count is its own table
 *   alone, and nothing written to the child reaches the parent. The child is
 *   the engine's (new-space &family-child (inherits &family-parent)), built
 *   as a term, the model C names through the language's own door.
 * Guarantees: all six claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include "spaces.h"

typedef struct fact {
    const char *head, *first, *second;
} fact;

static const fact parent_facts[] = { { "edge", "a", "b" }, { "parent-only", "kept", NULL }, { "layer", "parent", NULL } };
static const fact child_facts[] = { { "edge", "b", "c" }, { "child-only", "local", NULL }, { "layer", "child", NULL } };
#define COUNT(table) (sizeof table / sizeof *table)

static mt_atom *atom_of(fact f) { return f.second ? E(f.head, f.first, f.second) : E(f.head, f.first); }

static void write_all(mt_space *space, const fact *facts, size_t n)
{
    for (size_t i = 0; i < n; i++) require(facts[i].head, mt_add(space, atom_of(facts[i])));
}

/* The chain's reading of one unary relation: the child's first arguments,
   then the parent's. */
static mt_list chain_read(const char *head)
{
    mt_list found = { mt_alloc(8 * sizeof *found.items), 0 };
    require("room", found.items != NULL);
    const fact *tables[] = { child_facts, parent_facts };
    const size_t sizes[] = { COUNT(child_facts), COUNT(parent_facts) };
    for (size_t t = 0; t < 2; t++)
        for (size_t i = 0; i < sizes[t]; i++)
            if (strcmp(tables[t][i].head, head) == 0) found.items[found.len++] = S(tables[t][i].first);
    return found;
}

/* The two-edge join over the union of both tables: (x z) for x -> y -> z. */
static mt_list joined(void)
{
    const fact *edges[COUNT(parent_facts) + COUNT(child_facts)];
    size_t n = 0;
    for (size_t i = 0; i < COUNT(parent_facts); i++) edges[n++] = &parent_facts[i];
    for (size_t i = 0; i < COUNT(child_facts); i++) edges[n++] = &child_facts[i];
    mt_list pairs = { mt_alloc(n * n * sizeof *pairs.items), 0 };
    require("room", pairs.items != NULL);
    for (size_t i = 0; i < n; i++)
        for (size_t j = 0; j < n; j++)
            if (strcmp(edges[i]->head, "edge") == 0 && strcmp(edges[j]->head, "edge") == 0 &&
                strcmp(edges[i]->second, edges[j]->first) == 0)
                pairs.items[pairs.len++] = E(edges[i]->first, edges[j]->second);
    return pairs;
}

/* What a query binds `name` to, each answer in turn. TAKES pattern. */
static mt_list bindings(mt_space *space, mt_atom *pattern, const char *first, const char *second)
{
    mt_list got = { NULL, 0 };
    mt_rows (row, mt_query(space, pattern, NULL)) {
        mt_atom **grown = mt_resize(got.items, (got.len + 1) * sizeof *got.items);
        require("room", grown != NULL);
        got.items = grown;
        got.items[got.len++] = second ? E(mt_keep(mt_bound(row, first)), mt_keep(mt_bound(row, second)))
                                      : mt_keep(mt_bound(row, first));
    }
    return got;
}

/* The engine's list against C's, each TAKEN. */
static void agree(const char *claim, mt_list got, mt_list want)
{
    check_list_(claim, got, want.len, want.items);
    mt_free(want.items);
}

int main(void)
{
    metta *m = open_engine();
    mt_space *parent = mt_space_open(m, "&family-parent");
    require("open the parent", parent != NULL);
    write_all(parent, parent_facts, COUNT(parent_facts));
    mt_space *child = new_space(m, "&family-child", E("inherits", mt_spaceref("&family-parent")));
    write_all(child, child_facts, COUNT(child_facts));

    agree("a join links a parent edge to a child edge",
          bindings(child, E(",", E("edge", V("x"), V("y")), E("edge", V("y"), V("z"))), "x", "z"), joined());
    agree("the chain reads the child first", bindings(child, E("layer", V("x")), "x", NULL), chain_read("layer"));
    check_int("the count is the child's own store", (int64_t)mt_count(child), (int64_t)COUNT(child_facts));
    check_answers("the parent keeps its fact", mt_query(parent, E("parent-only", V("x")), NULL), E("parent-only", "kept"));
    check_answers("the child reads it through the chain", mt_query(child, E("parent-only", V("x")), NULL),
                  E("parent-only", "kept"));
    check_none("and nothing the child wrote reached the parent", mt_query(parent, E("child-only", V("x")), NULL));
    mt_space_close(child);
    mt_space_close(parent);
    return done(m);
}
