/* Purpose: the 2-3 finger tree's internals, held against the Hinze-Paterson
 *   operations written in C over the same atoms. A tree is FTEmpty,
 *   (FTSingle x) or (FTDeep prefix middle suffix), a digit one to four
 *   elements and a middle a tree of FTNode2 and FTNode3 one level down, so
 *   flattening knows its level and opens a node only below level zero. The
 *   left view takes a prefix's first element, and when that empties the
 *   prefix it borrows: from the middle's own left view, or from the suffix
 *   when the middle is empty, which is ft-borrow-l; the right view mirrors
 *   it. nodes regroups loose elements three at a time, leaving two, three or
 *   four for the last rows, and a pushed or appended tree flattens to the
 *   lists concatenated [source: Hinze and Paterson, "Finger trees: a simple
 *   general-purpose data structure", JFP 16(2), 2006, sections 3 and 4].
 * Guarantees: all twenty-eight claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

enum { MOST = 16 };

typedef struct seq { size_t n; mt_atom *item[MOST]; } seq;

static void push(seq *s, mt_atom *x)
{
    require("room in the sequence", s->n < MOST);
    s->item[s->n++] = x;
}

static bool headed(const mt_atom *t, const char *name)
{
    return (mt_kind_of(t) == MT_SYMBOL && strcmp(mt_name(t), name) == 0) ||
           (mt_len(t) > 0 && mt_kind_of(mt_at(t, 0)) == MT_SYMBOL && strcmp(mt_name(mt_at(t, 0)), name) == 0);
}

/* An element at a level: itself at level zero, a node's elements below it. */
static void expand(const mt_atom *x, size_t level, seq *out)
{
    if (level == 0) {
        push(out, mt_keep(x));
        return;
    }
    for (size_t i = 1; i < mt_len(x); i++) expand(mt_at(x, i), level - 1, out);
}

static void flatten(const mt_atom *t, size_t level, seq *out)
{
    if (headed(t, "FTEmpty") && mt_kind_of(t) == MT_SYMBOL) return;
    if (headed(t, "FTSingle")) {
        expand(mt_at(t, 1), level, out);
        return;
    }
    const mt_atom *prefix = mt_at(t, 1), *middle = mt_at(t, 2), *suffix = mt_at(t, 3);
    for (size_t i = 0; i < mt_len(prefix); i++) expand(mt_at(prefix, i), level, out);
    flatten(middle, level + 1, out);
    for (size_t i = 0; i < mt_len(suffix); i++) expand(mt_at(suffix, i), level, out);
}

static mt_atom *as_list(seq s) { return mt_exprv(s.n, s.item); }

static mt_atom *to_list(const mt_atom *t)
{
    seq s = { 0 };
    flatten(t, 0, &s);
    return as_list(s);
}

static mt_atom *slice(const mt_atom *digit, size_t from, size_t to)
{
    mt_atom *kids[MOST];
    for (size_t i = from; i < to; i++) kids[i - from] = mt_keep(mt_at(digit, i));
    return mt_exprv(to - from, kids);
}

/* A digit as a tree: one element is a single, more split in halves around an
   empty middle. */
static mt_atom *digit_tree(const mt_atom *digit)
{
    size_t n = mt_len(digit), half = n / 2;
    if (n == 1) return E("FTSingle", mt_keep(mt_at(digit, 0)));
    return E("FTDeep", slice(digit, 0, half), "FTEmpty", slice(digit, half, n));
}

static mt_atom *node_digit(const mt_atom *node) { return slice(node, 1, mt_len(node)); }

static bool is_empty(const mt_atom *t) { return mt_kind_of(t) == MT_SYMBOL && headed(t, "FTEmpty"); }

static mt_atom *borrow_left(const mt_atom *middle, const mt_atom *suffix);
static mt_atom *borrow_right(const mt_atom *prefix, const mt_atom *middle);

/* The left view: the first element and the tree after it. */
static void view_left(const mt_atom *t, mt_atom **first, mt_atom **rest)
{
    if (headed(t, "FTSingle")) {
        *first = mt_keep(mt_at(t, 1));
        *rest = mt_sym("FTEmpty");
        return;
    }
    const mt_atom *prefix = mt_at(t, 1), *middle = mt_at(t, 2), *suffix = mt_at(t, 3);
    *first = mt_keep(mt_at(prefix, 0));
    *rest = mt_len(prefix) > 1 ? E("FTDeep", slice(prefix, 1, mt_len(prefix)), mt_keep(middle), mt_keep(suffix))
                               : borrow_left(middle, suffix);
}

static void view_right(const mt_atom *t, mt_atom **rest, mt_atom **last)
{
    if (headed(t, "FTSingle")) {
        *last = mt_keep(mt_at(t, 1));
        *rest = mt_sym("FTEmpty");
        return;
    }
    const mt_atom *prefix = mt_at(t, 1), *middle = mt_at(t, 2), *suffix = mt_at(t, 3);
    size_t n = mt_len(suffix);
    *last = mt_keep(mt_at(suffix, n - 1));
    *rest = n > 1 ? E("FTDeep", mt_keep(prefix), mt_keep(middle), slice(suffix, 0, n - 1))
                  : borrow_right(prefix, middle);
}

/* A drained prefix: the suffix becomes the tree when the middle is empty,
   otherwise the middle's first node drops a level to be the prefix. */
static mt_atom *borrow_left(const mt_atom *middle, const mt_atom *suffix)
{
    if (is_empty(middle)) return digit_tree(suffix);
    mt_atom *node, *rest;
    view_left(middle, &node, &rest);
    mt_atom *tree = E("FTDeep", node_digit(node), rest, mt_keep(suffix));
    mt_drop(node);
    return tree;
}

static mt_atom *borrow_right(const mt_atom *prefix, const mt_atom *middle)
{
    if (is_empty(middle)) return digit_tree(prefix);
    mt_atom *node, *rest;
    view_right(middle, &rest, &node);
    mt_atom *tree = E("FTDeep", mt_keep(prefix), rest, node_digit(node));
    mt_drop(node);
    return tree;
}

/* Loose elements as 2-3 nodes, three at a time until two, three or four are
   left for the last rows, so no node is a singleton. */
static mt_atom *nodes(const int64_t *xs, size_t n)
{
    mt_atom *out[MOST];
    size_t k = 0, i = 0;
    while (n - i > 4) {
        out[k++] = E("FTNode3", xs[i], xs[i + 1], xs[i + 2]);
        i += 3;
    }
    switch (n - i) {
    case 2: out[k++] = E("FTNode2", xs[i], xs[i + 1]); break;
    case 3: out[k++] = E("FTNode3", xs[i], xs[i + 1], xs[i + 2]); break;
    case 4:
        out[k++] = E("FTNode2", xs[i], xs[i + 1]);
        out[k++] = E("FTNode2", xs[i + 2], xs[i + 3]);
        break;
    }
    return mt_exprv(k, out);
}

/* Lists joined, what every push, append and concatenation flattens to. */
static mt_atom *joined(const int64_t *a, size_t n, const int64_t *b, size_t m, const int64_t *c, size_t k)
{
    int64_t all[MOST];
    memcpy(all, a, n * sizeof *a);
    memcpy(all + n, b, m * sizeof *b);
    memcpy(all + n + m, c, k * sizeof *c);
    return mt_array(n + m + k, all);
}

static mt_atom *from_list(const int64_t *xs, size_t n) { return E("ft-from-list", mt_array(n, xs)); }

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_datastructures",
            mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_datastructures")))));

    mt_atom *single = E("FTSingle", 1), *deep = E("FTDeep", E(1, 2), "FTEmpty", E(3, 4));
    assert(answers_are(mt_eval(m, E("ft-to-list", mt_keep(single))), E(to_list(single))) && "a single flattens");
    assert(answers_are(mt_eval(m, E("ft-to-list", mt_keep(deep))), E(to_list(deep))) && "a deep tree written by hand");
    mt_atom *empty = mt_sym("FTEmpty");
    assert(answers_are(mt_eval(m, E("ft-is-empty", mt_keep(empty))), E(B(is_empty(empty)))) && "FTEmpty is empty");
    assert(answers_are(mt_eval(m, E("ft-is-empty", mt_keep(single))), E(B(is_empty(single)))) && "a single is not");

    mt_atom *node2 = E("FTNode2", "a", "b"), *node3 = E("FTNode3", "a", "b", "c");
    assert(answers_are(mt_eval(m, E("ft-node-digit", mt_keep(node2))), E(node_digit(node2))) && "a node's digit");
    assert(answers_are(mt_eval(m, E("ft-node-digit", mt_keep(node3))), E(node_digit(node3))) && "of three");

    struct { const char *claim; mt_atom *middle, *suffix; } lefts[] = {
        { "an empty middle and one suffix element", mt_sym("FTEmpty"), E("a") },
        { "two", mt_sym("FTEmpty"), E("a", "b") },
        { "four", mt_sym("FTEmpty"), E("a", "b", "c", "d") },
        { "a node borrowed from the middle", E("FTSingle", E("FTNode2", "a", "b")), E("c") },
    };
    for (size_t i = 0; i < sizeof lefts / sizeof *lefts; i++) {
        assert(answers_are(mt_eval(m, E("ft-borrow-l", mt_keep(lefts[i].middle), mt_keep(lefts[i].suffix))), E(borrow_left(lefts[i].middle, lefts[i].suffix)))
               && lefts[i].claim);
        mt_drop(lefts[i].middle);
        mt_drop(lefts[i].suffix);
    }
    struct { const char *claim; mt_atom *prefix, *middle; } rights[] = {
        { "its mirror: one prefix element", E("a"), mt_sym("FTEmpty") },
        { "two", E("a", "b"), mt_sym("FTEmpty") },
        { "a node borrowed from the middle's back", E("a"), E("FTSingle", E("FTNode2", "b", "c")) },
    };
    for (size_t i = 0; i < sizeof rights / sizeof *rights; i++) {
        assert(answers_are(mt_eval(m, E("ft-borrow-r", mt_keep(rights[i].prefix), mt_keep(rights[i].middle))), E(borrow_right(rights[i].prefix, rights[i].middle)))
               && rights[i].claim);
        mt_drop(rights[i].prefix);
        mt_drop(rights[i].middle);
    }
    mt_atom *middle3 = E("FTSingle", E("FTNode3", "a", "b", "c")), *d = E("d");
    mt_atom *borrowed = borrow_left(middle3, d);
    assert(answers_are(mt_eval(m, E("ft-to-list", E("ft-borrow-l", mt_keep(middle3), mt_keep(d)))), E(to_list(borrowed)))
           && "one node moved one level keeps every element");
    mt_drop(borrowed);

    static const int64_t six[] = { 1, 2, 3, 4, 5, 6 };
    for (size_t n = 2; n <= 6; n++)
        assert(answers_are(mt_eval(m, E("ft-nodes", mt_array(n, six))), E(nodes(six, n))) && "nodes regroups loose elements");

    static const int64_t one_two[] = { 1, 2 }, three_four[] = { 3, 4 }, seven_eight[] = { 7, 8 };
    assert(answers_are(mt_eval(m, E("ft-to-list", E("ft-push-list-front", mt_array(2, one_two), from_list(three_four, 2)))), E(joined(one_two, 2, three_four, 2, NULL, 0)))
           && "pushing a list on the front keeps its order");
    assert(answers_are(mt_eval(m, E("ft-to-list", E("ft-push-list-back", mt_array(2, one_two), from_list(three_four, 2)))), E(joined(three_four, 2, one_two, 2, NULL, 0)))
           && "pushing it on the back puts it after");
    assert(answers_are(mt_eval(m, E("ft-to-list", E("ft-push-list-front", mt_unit(), from_list(three_four, 2)))), E(joined(three_four, 2, NULL, 0, NULL, 0)))
           && "pushing nothing on the front");
    assert(answers_are(mt_eval(m, E("ft-to-list", E("ft-push-list-back", mt_unit(), from_list(three_four, 2)))), E(joined(three_four, 2, NULL, 0, NULL, 0)))
           && "or the back");
    assert(answers_are(mt_eval(m, E("ft-to-list", E("ft-app3", from_list(one_two, 2), mt_array(2, seven_eight), from_list(three_four, 2)))), E(joined(one_two, 2, seven_eight, 2, three_four, 2)))
           && "app3 joins two trees around loose elements");
    assert(answers_are(mt_eval(m, E("ft-to-list", E("ft-app3", "FTEmpty", mt_array(2, seven_eight), from_list(three_four, 2)))), E(joined(NULL, 0, seven_eight, 2, three_four, 2)))
           && "an empty left tree");
    assert(answers_are(mt_eval(m, E("ft-to-list", E("ft-app3", from_list(one_two, 2), mt_unit(), "FTEmpty"))), E(joined(one_two, 2, NULL, 0, NULL, 0)))
           && "an empty right tree");
    assert(answers_are(mt_eval(m, E("ft-to-list", E("ft-app3", "FTEmpty", mt_unit(), "FTEmpty"))), E(joined(NULL, 0, NULL, 0, NULL, 0)))
           && "nothing at all");
    mt_atom *concat = joined(one_two, 2, NULL, 0, three_four, 2), *app3 = joined(one_two, 2, NULL, 0, three_four, 2);
    assert(answers_are(mt_eval(m, E("==", E("ft-to-list", E("ft-concat", from_list(one_two, 2), from_list(three_four, 2))),
                                    E("ft-to-list", E("ft-app3", from_list(one_two, 2), mt_unit(), from_list(three_four, 2))))), E(B(mt_eq(concat, app3))))
           && "concat is app3 with nothing between");
    mt_drop(concat);
    mt_drop(app3);

    mt_drop(single); mt_drop(deep); mt_drop(empty); mt_drop(node2); mt_drop(node3); mt_drop(middle3); mt_drop(d);
    mt_close(m);
    return 0;
}
