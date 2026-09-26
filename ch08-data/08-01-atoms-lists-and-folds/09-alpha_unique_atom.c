/* Purpose: dedupe modulo renaming. alpha-unique-atom keeps the first of
 *   each class of alpha-equal elements; C does the same with a loop over
 *   the children and mt_alpha_eq against the ones it kept. Each of the
 *   original's thirteen lists is a row: the engine's answer must be
 *   alpha-equal to the original's expectation, and C's to the engine's.
 * Guarantees: all thirteen claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>

static mt_atom *first_of_each(const mt_atom *items)
{
    mt_atom *kept[16];
    size_t n = 0;
    for (size_t i = 0; i < mt_len(items); i++) {
        bool seen = false;
        for (size_t j = 0; j < n && !seen; j++) seen = mt_alpha_eq(kept[j], mt_at(items, i));
        if (!seen && n < 16) kept[n++] = mt_keep(mt_at(items, i));
    }
    return mt_exprv(n, kept);
}

static mt_atom *link(mt_atom *x) { return E("link", x, "human"); }
static mt_atom *parent(mt_atom *x) { return E("parent", x, "human"); }
static mt_atom *child(mt_atom *x) { return E("child", x, "human"); }

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    const struct { const char *claim; mt_atom *items, *expected; } rows[] = {
        { "three renamings of one link", E(link(V("x")), link(V("y")), link(V("z"))), E(link(V("a"))) },
        { "two functors, one repeated", E(parent(V("x")), parent(V("y")), child(V("z"))), E(parent(V("a")), child(V("b"))) },
        { "three functors", E(parent(V("x")), child(V("y")), E("friend", V("z"), "human")),
          E(parent(V("a")), child(V("b")), E("friend", V("c"), "human")) },
        { "three one-place facts", E(E("likes", V("x")), E("hates", V("y")), E("knows", V("z"))),
          E(E("likes", V("a")), E("hates", V("b")), E("knows", V("c"))) },
        { "nested structure", E(link(E("foo", V("x"))), link(E("foo", V("y"))), link(E("bar", V("z")))),
          E(link(E("foo", V("a"))), link(E("bar", V("b")))) },
        { "a nested variable repeated", E(parent(E("child", V("x"))), parent(E("child", V("y"))), parent(E("child", V("x")))),
          E(parent(E("child", V("a")))) },
        { "a mix, first of each", E(link(V("x")), parent(V("x")), link(V("y")), parent(V("z")), link(V("x"))),
          E(link(V("a")), parent(V("a"))) },
        { "and another", E(E("foo", V("x")), E("foo", V("y")), E("bar", V("x")), E("foo", V("x")), E("bar", V("y"))),
          E(E("foo", V("a")), E("bar", V("a"))) },
        { "numbers", E(1, 2, 2, 3, 1, 4, 4, 5), E(1, 2, 3, 4, 5) },
        { "symbols", E("a", "b", "a", "c", "b", "d", "e", "a"), E("a", "b", "c", "d", "e") },
        { "the empty list", mt_unit(), mt_unit() },
        { "one element", E(1), E(1) },
        { "one fact", E(link(V("x"))), E(link(V("a"))) },
    };
    for (size_t i = 0; i < sizeof rows / sizeof *rows; i++) {
        mt_atom *engine = mt_one(mt_eval(m, E("alpha-unique-atom", mt_keep(rows[i].items))));
        mt_atom *mine = first_of_each(rows[i].items);
        assert(engine && mt_alpha_eq(engine, rows[i].expected) && mt_alpha_eq(mine, engine) && rows[i].claim);
        mt_drop(engine);
        mt_drop(mine);
        mt_drop(rows[i].items);
        mt_drop(rows[i].expected);
    }
    mt_close(m);
    return 0;
}
