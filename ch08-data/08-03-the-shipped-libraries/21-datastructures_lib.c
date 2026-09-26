/* Purpose: lib_datastructures, held against the same structures in C. A map
 *   and a priority queue are both rows of (Key Value) pairs, C arrays kept in
 *   the standard order of terms by mt_compare, cmetta's copy of the engine's
 *   order: a map sorted by key with no key twice, where a key is found by
 *   identity, so two variables are two keys; a queue stable-sorted by
 *   priority, so equal priorities keep the order they came in. Both are
 *   values: every operation builds new rows and leaves its input alone, and
 *   C compares the engine's answer with the rows it built, tagged SortedMap
 *   or PriorityQueue as the library tags them. The functional queue is
 *   Okasaki's two stacks in C arrays, and the finger tree's claims are about
 *   the sequence it holds, which in C is an array. Where the library
 *   refuses, a precondition C states refuses the same input.
 * Guarantees: all fifty-nine claims of the original hold
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

/* Whether an atom is want up to renaming variables; takes both, and shows
   them when they differ. */
static inline bool atom_is(mt_atom *got, mt_atom *want)
{
    bool holds = got && want && mt_alpha_eq(got, want);
    if (!holds) fprintf(stderr, "  got %s\n  want %s\n", got ? mt_show(got) : "nothing",
                        want ? mt_show(want) : "nothing");
    mt_drop(got);
    mt_drop(want);
    return holds;
}

enum { MOST = 16 };

typedef struct row {
    mt_atom *key, *value;
} row;

typedef struct rows {
    row at[MOST];
    size_t n;
} rows;

static void add_row(rows *r, const mt_atom *key, const mt_atom *value)
{
    require("room for the rows", r->n < MOST);
    r->at[r->n++] = (row){ mt_keep(key), mt_keep(value) };
}

static void rows_free(rows *r)
{
    for (; r->n; r->n--) {
        mt_drop(r->at[r->n - 1].key);
        mt_drop(r->at[r->n - 1].value);
    }
}

static int by_key(const void *a, const void *b) { return mt_compare(((const row *)a)->key, ((const row *)b)->key); }

/* Rows from an expression of pairs; false when a child is no pair. */
static bool read_rows(const mt_atom *pairs, rows *out)
{
    out->n = 0;
    for (size_t i = 0; i < mt_len(pairs); i++) {
        const mt_atom *pair = mt_at(pairs, i);
        if (mt_kind_of(pair) != MT_EXPR || mt_len(pair) != 2) return rows_free(out), false;
        add_row(out, mt_at(pair, 0), mt_at(pair, 1));
    }
    return true;
}

/* A map: the rows in key order; false when a key repeats, since two values
   for one key is no map. */
static bool map_of(const mt_atom *pairs, rows *out)
{
    if (!read_rows(pairs, out)) return false;
    qsort(out->at, out->n, sizeof *out->at, by_key);
    for (size_t i = 1; i < out->n; i++)
        if (by_key(&out->at[i - 1], &out->at[i]) == 0) return rows_free(out), false;
    return true;
}

/* A queue: the rows in priority order, an insertion sort, which is stable.
   Time: n^2 / 2 comparisons at worst, n = rows. */
static rows queued(rows r)
{
    for (size_t i = 1; i < r.n; i++)
        for (size_t j = i; j > 0 && by_key(&r.at[j - 1], &r.at[j]) > 0; j--) {
            row swap = r.at[j];
            r.at[j] = r.at[j - 1];
            r.at[j - 1] = swap;
        }
    return r;
}

static rows queue_of(const mt_atom *pairs)
{
    rows r;
    require("a queue's rows are pairs", read_rows(pairs, &r));
    return queued(r);
}

/* Reading the rows back out as the library answers them. */
static mt_atom *pair_at(const rows *r, size_t i) { return E(mt_keep(r->at[i].key), mt_keep(r->at[i].value)); }

static mt_atom *column(const rows *r, int which)
{
    mt_atom *kids[MOST];
    for (size_t i = 0; i < r->n; i++)
        kids[i] = which < 0 ? pair_at(r, i) : mt_keep(which ? r->at[i].value : r->at[i].key);
    return mt_exprv(r->n, kids);
}

static mt_atom *pairs_of(const rows *r) { return column(r, -1); }
static mt_atom *tagged(const char *tag, const rows *r) { return E(tag, pairs_of(r)); }

/* Reading answers C built as values: each TAKES the rows it reads. */
static mt_atom *pairs_done(rows r)
{
    mt_atom *out = pairs_of(&r);
    rows_free(&r);
    return out;
}

static int64_t size_done(rows r)
{
    int64_t n = (int64_t)r.n;
    rows_free(&r);
    return n;
}

static const mt_atom *lookup(const rows *r, const mt_atom *key)
{
    for (size_t i = 0; i < r->n; i++)
        if (mt_compare(r->at[i].key, key) == 0) return r->at[i].value;
    return NULL;
}

static rows put(const rows *r, const mt_atom *key, const mt_atom *value)
{
    rows out = { .n = 0 };
    for (size_t i = 0; i < r->n; i++)
        if (mt_compare(r->at[i].key, key) != 0) add_row(&out, r->at[i].key, r->at[i].value);
    add_row(&out, key, value);
    qsort(out.at, out.n, sizeof *out.at, by_key);
    return out;
}

static rows without(const rows *r, const mt_atom *key)
{
    rows out = { .n = 0 };
    for (size_t i = 0; i < r->n; i++)
        if (mt_compare(r->at[i].key, key) != 0) add_row(&out, r->at[i].key, r->at[i].value);
    return out;
}

/* Every row of every queue, left to right, then priority order. */
static rows merged(const rows *const *queues, size_t count)
{
    rows out = { .n = 0 };
    for (size_t q = 0; q < count; q++)
        for (size_t i = 0; i < queues[q]->n; i++) add_row(&out, queues[q]->at[i].key, queues[q]->at[i].value);
    return queued(out);
}

static rows inserted(const rows *q, const mt_atom *priority, const mt_atom *value)
{
    rows extra = { .n = 0 };
    add_row(&extra, priority, value);
    const rows *both[] = { q, &extra };
    rows out = merged(both, 2);
    rows_free(&extra);
    return out;
}

/* The queue without its first entry of this identical priority and value;
   false when there is none. */
static bool cancelled(const rows *q, const mt_atom *priority, const mt_atom *value, rows *out)
{
    out->n = 0;
    bool gone = false;
    for (size_t i = 0; i < q->n; i++) {
        if (!gone && mt_compare(q->at[i].key, priority) == 0 && mt_compare(q->at[i].value, value) == 0) gone = true;
        else add_row(out, q->at[i].key, q->at[i].value);
    }
    if (!gone) rows_free(out);
    return gone;
}

/* A value of the library's kind: (SortedMap rows) or (PriorityQueue rows). */
static bool of_kind(const mt_atom *v, const char *tag)
{
    return mt_kind_of(v) == MT_EXPR && mt_len(v) == 2 && mt_kind_of(mt_at(v, 0)) == MT_SYMBOL &&
           strcmp(mt_name(mt_at(v, 0)), tag) == 0 && mt_kind_of(mt_at(v, 1)) == MT_EXPR;
}

static mt_atom *verdict(bool holds) { return S(holds ? "fine" : "refused"); }
static mt_atom *guarded(mt_atom *goal) { return E("if-error", E("catch", goal), "refused", "fine"); }

static bool map_ok(const mt_atom *pairs)
{
    rows r;
    bool ok = map_of(pairs, &r);
    if (ok) rows_free(&r);
    return ok;
}

/* Okasaki's queue: enqueue pushes the back stack; dequeue pops the front
   stack, first reversing the back stack into it when it is empty. Each stack
   reads top first, as the library's cons lists do. */
typedef struct fifo {
    int64_t back[MOST], front[MOST];
    size_t nback, nfront;
} fifo;

static void enqueue(fifo *q, int64_t x) { q->back[q->nback++] = x; }

static int64_t dequeue(fifo *q)
{
    if (!q->nfront)
        while (q->nback) q->front[q->nfront++] = q->back[--q->nback];
    return q->front[--q->nfront];
}

static mt_atom *top_first(const int64_t *stack, size_t n)
{
    mt_atom *kids[MOST];
    for (size_t i = 0; i < n; i++) kids[i] = mt_num(stack[n - 1 - i]);
    return mt_exprv(n, kids);
}

static mt_atom *fifo_atom(const fifo *q)
{
    return E("queue", top_first(q->back, q->nback), top_first(q->front, q->nfront), (int64_t)(q->nback + q->nfront));
}

static mt_atom *value_of(metta *m, mt_atom *goal)
{
    mt_atom *v = mt_one(mt_eval(m, goal));
    require("a value", v != NULL);
    return v;
}

/* The function an equation of `head` spells, read back out of &self and
   evaluated: (|-> params body) over the head's variables named in params. */
static mt_atom *recipe(metta *m, mt_atom *head, const char *const *params, size_t count)
{
    mt_atom *lambda = NULL;
    mt_rows (row, mt_match(m, E("=", head, V("body")))) {
        mt_atom *names[4];
        for (size_t i = 0; i < count; i++) names[i] = mt_keep(mt_bound(row, params[i]));
        mt_drop(lambda);
        lambda = E("|->", mt_exprv(count, names), mt_keep(mt_bound(row, "body")));
    }
    require("the equation is in &self", lambda != NULL);
    return value_of(m, lambda);
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_datastructures",
            mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_datastructures")))));

    /* A map from pairs, in the standard order however the pairs came. */
    mt_atom *fruit = E(E("pear", 5), E("apple", 3), E("plum", 7)), *pear = S("pear"), *durian = S("durian");
    rows book;
    require("C's map", map_of(fruit, &book));
    mt_atom *prices = value_of(m, E("map-from-pairs", mt_keep(fruit)));
    assert(answers_are(mt_eval(m, E("map-size", mt_keep(prices))), E((int64_t)book.n)) && "map-size");
    assert(answers_are(mt_eval(m, E("map-keys", mt_keep(prices))), E(column(&book, 0))) && "map-keys in order");
    assert(answers_are(mt_eval(m, E("map-values", mt_keep(prices))), E(column(&book, 1))) && "map-values");
    assert(answers_are(mt_eval(m, E("map-pairs", mt_keep(prices))), E(pairs_of(&book))) && "map-pairs");

    /* A lookup that finds nothing has no answer. */
    assert(answers_are(mt_eval(m, E("map-get", mt_keep(prices), mt_keep(pear))), E(mt_keep(lookup(&book, pear)))) && "map-get");
    require("durian is absent", lookup(&book, durian) == NULL);
    assert(!mt_first(mt_eval(m, E("map-get", mt_keep(prices), mt_keep(durian)))) && mt_ok() && "an absent key has no answer");
    assert(answers_are(mt_eval(m, E("map-get-or", mt_keep(prices), mt_keep(durian), 0)), E((int64_t)0)) && "map-get-or");
    mt_atom *apple = S("apple");
    assert(answers_are(mt_eval(m, E("map-has", mt_keep(prices), mt_keep(apple))), E(B(lookup(&book, apple) != NULL))) && "map-has");
    assert(answers_are(mt_eval(m, E("map-has", mt_keep(prices), mt_keep(durian))), E(B(lookup(&book, durian) != NULL))) && "map-has nothing");
    assert(answers_are(mt_eval(m, E("map-min", mt_keep(prices))), E(pair_at(&book, 0))) && "map-min");
    assert(answers_are(mt_eval(m, E("map-max", mt_keep(prices))), E(pair_at(&book, book.n - 1))) && "map-max");

    /* A put answers a new map and leaves the old one alone. */
    mt_atom *six = mt_num(6), *raised = value_of(m, E("map-put", mt_keep(prices), mt_keep(pear), mt_keep(six)));
    assert(answers_are(mt_eval(m, E("map-pairs", mt_keep(raised))), E(pairs_done(put(&book, pear, six)))) && "map-put");
    assert(answers_are(mt_eval(m, E("map-pairs", mt_keep(prices))), E(pairs_of(&book))) && "the old map stays");
    assert(answers_are(mt_eval(m, E("map-pairs", E("map-remove", mt_keep(prices), mt_keep(pear)))), E(pairs_done(without(&book, pear))))
           && "map-remove");
    assert(answers_are(mt_eval(m, E("map-pairs", E("map-remove", mt_keep(prices), mt_keep(durian)))), E(pairs_done(without(&book, durian))))
           && "removing an absent key");
    rows none = { .n = 0 };
    assert(answers_are(mt_eval(m, E("map-size", E("map-empty"))), E((int64_t)none.n)) && "the empty map");
    mt_atom *k = S("k"), *v = S("v");
    assert(answers_are(mt_eval(m, E("map-pairs", E("map-put", E("map-empty"), mt_keep(k), mt_keep(v)))), E(pairs_done(put(&none, k, v))))
           && "a put on the empty map");
    mt_atom *twice = E(E("k", 1), E("k", 2));
    assert(answers_are(mt_eval(m, guarded(E("map-from-pairs", mt_keep(twice)))), E(verdict(map_ok(twice)))) && "a key holds one value");
    mt_atom *number = mt_num(42);
    assert(answers_are(mt_eval(m, guarded(E("map-get", mt_keep(number), mt_keep(k)))), E(verdict(of_kind(number, "SortedMap"))))
           && "a number is no map");

    /* A queue keeps every occurrence in priority order. */
    mt_atom *chores = E(E(3, "sweep"), E(1, "wake"), E(2, "boil"));
    rows jobs = queue_of(chores);
    mt_atom *work = value_of(m, E("pq-from-pairs", mt_keep(chores)));
    assert(answers_are(mt_eval(m, E("pq-size", mt_keep(work))), E((int64_t)jobs.n)) && "pq-size");
    assert(answers_are(mt_eval(m, E("pq-min", mt_keep(work))), E(pair_at(&jobs, 0))) && "pq-min");
    assert(answers_are(mt_eval(m, E("pq-pairs", mt_keep(work))), E(pairs_of(&jobs))) && "pq-pairs");

    /* pq-pop answers the first entry and the rest together; C reads the
       triple apart with mt_at. */
    mt_atom *popped = value_of(m, E("pq-pop", mt_keep(work)));
    rows rest = { .n = 0 };
    for (size_t i = 1; i < jobs.n; i++) add_row(&rest, jobs.at[i].key, jobs.at[i].value);
    assert(atom_is(E(mt_keep(mt_at(popped, 0)), mt_keep(mt_at(popped, 1))), pair_at(&jobs, 0)) && "the first priority and value");
    assert(answers_are(mt_eval(m, E("pq-size", mt_keep(mt_at(popped, 2)))), E((int64_t)rest.n)) && "the rest is one entry shorter");
    assert(answers_are(mt_eval(m, E("pq-pairs", mt_keep(mt_at(popped, 2)))), E(pairs_of(&rest))) && "and holds the others");
    assert(answers_are(mt_eval(m, E("pq-size", mt_keep(work))), E((int64_t)jobs.n)) && "the queue popped stays whole");
    mt_drop(popped);
    rows_free(&rest);

    mt_atom *tie = E(E(1, "a"), E(1, "b")), *zero = mt_num(0), *rise = S("rise"), *two = mt_num(2), *boil = S("boil");
    assert(answers_are(mt_eval(m, E("pq-size", E("pq-from-pairs", mt_keep(tie)))), E(size_done(queue_of(tie))))
           && "repeated priorities are kept");
    assert(answers_are(mt_eval(m, E("pq-pairs", E("pq-insert", mt_keep(work), mt_keep(zero), mt_keep(rise)))), E(pairs_done(inserted(&jobs, zero, rise))))
           && "pq-insert");
    mt_atom *early = E(E(0, "rise"));
    rows dawn = queue_of(early);
    const rows *work_and_dawn[] = { &jobs, &dawn };
    assert(answers_are(mt_eval(m, E("pq-pairs", E("pq-merge", mt_keep(work), E("pq-from-pairs", mt_keep(early))))), E(pairs_done(merged(work_and_dawn, 2))))
           && "pq-merge");
    rows kept;
    require("C cancels boil", cancelled(&jobs, two, boil, &kept));
    assert(answers_are(mt_eval(m, E("pq-pairs", E("pq-remove", mt_keep(work), mt_keep(two), mt_keep(boil)))), E(pairs_done(kept))) && "pq-remove");
    mt_atom *absent = S("absent");
    require("C finds no absent entry", !cancelled(&jobs, two, absent, &kept));
    assert(!mt_first(mt_eval(m, E("pq-remove", mt_keep(work), mt_keep(two), absent))) && mt_ok() && "an absent entry has no answer");
    assert(answers_are(mt_eval(m, E("pq-size", E("pq-empty"))), E((int64_t)none.n)) && "the empty queue");
    require("C's empty queue has no minimum", none.n == 0);
    assert(!mt_first(mt_eval(m, E("pq-min", E("pq-empty")))) && mt_ok() && "so it has no minimum");
    assert(!mt_first(mt_eval(m, E("pq-pop", E("pq-empty")))) && mt_ok() && "and nothing to pop");
    assert(answers_are(mt_eval(m, guarded(E("pq-size", mt_keep(number)))), E(verdict(of_kind(number, "PriorityQueue")))) && "a number is no queue");

    /* The functional queue. */
    fifo q = { .nback = 0 };
    enqueue(&q, 1);
    enqueue(&q, 2);
    assert(answers_are(mt_eval(m, E("enqueue", 2, E("enqueue", 1, E("empty-queue")))), E(fifo_atom(&q))) && "enqueue");
    require("dequeue gives the first in", dequeue(&q) == 1);
    assert(answers_are(mt_eval(m, E("dequeue", 1, E("enqueue", 2, E("enqueue", 1, E("empty-queue"))))), E(fifo_atom(&q))) && "dequeue");

    /* The finger tree holds a sequence: C's is an array. */
    static const int64_t three[] = { 1, 2, 3 }, left[] = { 1, 2 }, right[] = { 3, 4 }, all[] = { 1, 2, 3, 4 };
    assert(answers_are(mt_eval(m, E("ft-to-list", E("ft-from-list", mt_array(3, three)))), E(mt_array(3, three))) && "ft-to-list inverts ft-from-list");
    assert(answers_are(mt_eval(m, E("ft-front", E("ft-from-list", mt_array(3, three)))), E(three[0])) && "ft-front");
    assert(answers_are(mt_eval(m, E("ft-back", E("ft-from-list", mt_array(3, three)))), E(three[2])) && "ft-back");
    assert(answers_are(mt_eval(m, E("ft-to-list", E("ft-concat", E("ft-from-list", mt_array(2, left)), E("ft-from-list", mt_array(2, right))))), E(mt_array(4, all)))
           && "ft-concat");
    assert(answers_are(mt_eval(m, E("ft-is-empty", E("ft-empty"))), E(B(none.n == 0))) && "ft-is-empty");

    /* A map is its relation, which C reads apart with mt_at. */
    assert(atom_is(mt_keep(prices), tagged("SortedMap", &book)) && "a map is the relation C builds");
    assert(answers_are(mt_eval(m, E("pairs-lookup", mt_keep(mt_at(prices, 1)), mt_keep(pear))), E(mt_keep(lookup(&book, pear))))
           && "whose rows the Pairs operations read");

    /* Merge takes zero, one or any number of queues. */
    assert(answers_are(mt_eval(m, E("pq-merge")), E(tagged("PriorityQueue", &none))) && "merging none");
    assert(answers_are(mt_eval(m, E("pq-merge", mt_keep(work))), E(tagged("PriorityQueue", &jobs))) && "merging one");
    const rows *thrice[] = { &jobs, &jobs, &jobs };
    assert(answers_are(mt_eval(m, E("pq-size", E("pq-merge", mt_keep(work), mt_keep(work), mt_keep(work)))), E(size_done(merged(thrice, 3))))
           && "merging three");
    assert(answers_are(mt_eval(m, E("pq-size", E("apply-to", "pq-merge", E("quote", E(mt_keep(work), mt_keep(work)))))), E(size_done(merged(thrice, 2))))
           && "merging a runtime sequence");
    mt_atom *ties = E(E(1, "a"), E(1, "b"), E(1, "c"));
    assert(answers_are(mt_eval(m, E("pq-pairs", E("pq-from-pairs", mt_keep(ties)))), E(pairs_done(queue_of(ties))))
           && "equal priorities keep their order");
    rows tied = queue_of(tie);
    mt_atom *one = mt_num(1), *c = S("c"), *a = S("a");
    assert(answers_are(mt_eval(m, E("pq-pairs", E("pq-insert", E("pq-from-pairs", mt_keep(tie)), mt_keep(one), mt_keep(c)))), E(pairs_done(inserted(&tied, one, c))))
           && "an insert goes after its equals");
    mt_atom *repeated = E(E(1, "a"), E(1, "a"), E(2, "b"));
    rows twins = queue_of(repeated);
    require("C cancels one (1 a)", cancelled(&twins, one, a, &kept));
    assert(answers_are(mt_eval(m, E("pq-pairs", E("pq-remove", E("pq-from-pairs", mt_keep(repeated)), mt_keep(one), mt_keep(a)))), E(pairs_done(kept)))
           && "a remove takes one occurrence");

    /* Variables keep their identity; malformed and cross-kind values refuse. */
    mt_atom *same_var = E(E(V("x"), "a"), E(V("x"), "b")), *loose = E(V("pair")), *two_vars = E(E(V("x"), "a"), E(V("y"), "b"));
    assert(answers_are(mt_eval(m, guarded(E("map-from-pairs", E("quote", mt_keep(same_var))))), E(verdict(map_ok(same_var)))) && "one variable is one key");
    assert(answers_are(mt_eval(m, guarded(E("map-from-pairs", E("quote", mt_keep(loose))))), E(verdict(map_ok(loose)))) && "a variable is no pair");
    rows vars;
    require("C's map of two variables", map_of(two_vars, &vars));
    mt_atom *x = V("x");
    assert(answers_are(mt_eval(m, E("let", V("map"), E("map-from-pairs", E("quote", mt_keep(two_vars))), E("map-get", V("map"), mt_keep(x)))), E(mt_keep(lookup(&vars, x))))
           && "two variables are two keys");
    mt_atom *sum = E("+", 1, 2), *literal = E(E(mt_keep(sum), E("Error", "data", "code")));
    rows data;
    require("C's map of a literal key", map_of(literal, &data));
    assert(answers_are(mt_eval(m, E("map-get", E("map-from-pairs", E("quote", mt_keep(literal))), E("quote", mt_keep(sum)))), E(mt_keep(lookup(&data, sum))))
           && "keys and values stay data");
    mt_atom *empty_queue = tagged("PriorityQueue", &none), *empty_map = tagged("SortedMap", &none);
    assert(answers_are(mt_eval(m, guarded(E("map-size", E("pq-empty")))), E(verdict(of_kind(empty_queue, "SortedMap")))) && "a queue is no map");
    assert(answers_are(mt_eval(m, guarded(E("pq-size", E("map-empty")))), E(verdict(of_kind(empty_map, "PriorityQueue")))) && "a map is no queue");
    mt_atom *choices[] = { E(E("a", 1)), E(E("a", 2)) }, *found[2];
    for (size_t i = 0; i < 2; i++) {
        rows alternative;
        require("C's map of each alternative", map_of(choices[i], &alternative));
        found[i] = mt_keep(lookup(&alternative, a));
        rows_free(&alternative);
    }
    assert(answers_are(mt_eval(m, E("map-get", E("map-from-pairs", E("superpose", E(mt_keep(choices[0]), mt_keep(choices[1])))), mt_keep(a))), E(found[0], found[1]))
           && "a map per alternative");

    /* An equation read back out of &self is the function, generally or
       specialized to one map. */
    const char *map_and_key[] = { "map", "key" }, *key_only[] = { "key" };
    mt_atom *remover = recipe(m, E("map-remove", V("map"), V("key")), map_and_key, 2);
    assert(answers_are(mt_eval(m, E("map-pairs", E(remover, mt_keep(prices), mt_keep(pear)))), E(pairs_done(without(&book, pear))))
           && "a recipe applied");
    mt_atom *plum = S("plum"), *specialized = recipe(m, E("map-remove", mt_keep(prices), V("key")), key_only, 1);
    assert(answers_are(mt_eval(m, E("map-pairs", E(specialized, mt_keep(plum)))), E(pairs_done(without(&book, plum))))
           && "a recipe specialized to one map");

    mt_atom *held[] = { fruit, pear, durian, apple, prices, six, raised, k, v, twice, number, chores, work, tie, zero, rise, two, boil,
                        early, ties, one, c, a, repeated, same_var, loose, two_vars, x, sum, literal, empty_queue, empty_map,
                        choices[0], choices[1], plum };
    for (size_t i = 0; i < sizeof held / sizeof *held; i++) mt_drop(held[i]);
    rows *models[] = { &book, &jobs, &dawn, &tied, &twins, &vars, &data };
    for (size_t i = 0; i < sizeof models / sizeof *models; i++) rows_free(models[i]);
    mt_close(m);
    return 0;
}
