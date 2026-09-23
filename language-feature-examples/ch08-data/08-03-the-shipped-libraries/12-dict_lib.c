/* Purpose: lib_dict's dictionaries, each a space of (key value) atoms, held
 *   against a C map: keys and values in two arrays, put replacing the value a
 *   key has, remove and pop taking it out, merge putting every pair of one
 *   map into another, update applying a C function to the value in place.
 *   Every operation the engine performs, C performs on its own map, and each
 *   answer must be what the C map says: a size, a membership, a value or the
 *   default, a pattern query's keys, the pairs sorted with mt_order.
 * Guarantees: all twenty-five claims of the original hold [tested: make
 *   twins; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

enum { MOST = 8 };

typedef struct dict { size_t n; const char *key[MOST]; int64_t value[MOST]; } dict;

static size_t find(const dict *d, const char *key)
{
    size_t i = 0;
    while (i < d->n && strcmp(d->key[i], key) != 0) i++;
    return i;
}

static bool has(const dict *d, const char *key) { return find(d, key) < d->n; }

/* A key maps to one value: putting a key that is there replaces it. */
static void put(dict *d, const char *key, int64_t value)
{
    size_t i = find(d, key);
    if (i == d->n) {
        require("room in the map", d->n < MOST);
        d->key[d->n++] = key;
    }
    d->value[i] = value;
}

static bool removed(dict *d, const char *key)
{
    size_t i = find(d, key);
    if (i == d->n) return false;
    d->n--;
    d->key[i] = d->key[d->n];
    d->value[i] = d->value[d->n];
    return true;
}

static int64_t get(const dict *d, const char *key, int64_t fallback)
{
    size_t i = find(d, key);
    return i < d->n ? d->value[i] : fallback;
}

static mt_atom *sorted(mt_atom **items, size_t n)
{
    qsort(items, n, sizeof *items, mt_order);
    return mt_exprv(n, items);
}

static mt_atom *pairs(const dict *d)
{
    mt_atom *items[MOST];
    for (size_t i = 0; i < d->n; i++) items[i] = E(mt_sym(d->key[i]), d->value[i]);
    return sorted(items, d->n);
}

static mt_atom *values(const dict *d)
{
    mt_atom *items[MOST];
    for (size_t i = 0; i < d->n; i++) items[i] = mt_num(d->value[i]);
    return sorted(items, d->n);
}

/* The value a key holds as a match answers it: one answer, or none. */
static mt_atom *lookup(const dict *d, const char *key)
{
    return has(d, key) ? E(get(d, key, 0)) : mt_unit();
}

static mt_atom *keys_holding(const dict *d, int64_t value)
{
    mt_atom *items[MOST];
    size_t n = 0;
    for (size_t i = 0; i < d->n; i++)
        if (d->value[i] == value) items[n++] = mt_sym(d->key[i]);
    return sorted(items, n);
}

static void merge(dict *into, const dict *from)
{
    for (size_t i = 0; i < from->n; i++) put(into, from->key[i], from->value[i]);
}

static int64_t add_six(int64_t v) { return v + 6; }

static void update(dict *d, const char *key, int64_t (*f)(int64_t))
{
    if (has(d, key)) put(d, key, f(get(d, key, 0)));
}

/* A dict-space over the same pairs. The original names it with bind!, which
   registers a token for its reader; a term C builds never meets that reader,
   so C names the space the way it names any value, in a variable holding the
   atom dict-space answers. */
static mt_atom *dict_space(metta *m, const dict *d)
{
    mt_atom *items[MOST];
    for (size_t i = 0; i < d->n; i++) items[i] = E(mt_sym(d->key[i]), d->value[i]);
    mt_atom *space = mt_one(mt_eval(m, E("dict-space", mt_exprv(d->n, items))));
    require("dict-space answers a space", space != NULL);
    return space;
}

int main(void)
{
    metta *m = open_engine();
    require("import lib_dict", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_dict")))));

    dict prices = { 2, { "apple", "pear" }, { 3, 5 } };
    mt_atom *prices_space = dict_space(m, &prices);
    check_answers("dict-size is the atom count", mt_eval(m, E("dict-size", mt_keep(prices_space))), (int64_t)prices.n);
    check_answers("dict-has a key", mt_eval(m, E("dict-has", mt_keep(prices_space), "apple")), B(has(&prices, "apple")));
    check_answers("and one that is not", mt_eval(m, E("dict-has", mt_keep(prices_space), "durian")), B(has(&prices, "durian")));
    check_answers("one value per key", mt_eval(m, E("sort-atom", E("collapse", E("dict-values", mt_keep(prices_space))))), values(&prices));
    check_answers("the pairs", mt_eval(m, E("sort-atom", E("dict-pairs", mt_keep(prices_space)))), pairs(&prices));

    put(&prices, "plum", 7);
    check_answers("dict-put writes and answers the dict", mt_eval(m, E("dict-size", E("dict-put", mt_keep(prices_space), "plum", 7))),
                  (int64_t)prices.n);
    check_answers("the space itself grew", mt_eval(m, E("dict-size", mt_keep(prices_space))), (int64_t)prices.n);
    check_answers("the new key matches", mt_eval(m, E("collapse", E("match", mt_keep(prices_space), E("plum", V("v")), V("v")))),
                  lookup(&prices, "plum"));
    put(&prices, "apple", 9);
    check_answers("a key put again is replaced", mt_eval(m, E("dict-size", E("dict-put", mt_keep(prices_space), "apple", 9))),
                  (int64_t)prices.n);
    check_answers("and holds the new value only", mt_eval(m, E("collapse", E("match", mt_keep(prices_space), E("apple", V("v")), V("v")))),
                  lookup(&prices, "apple"));

    removed(&prices, "pear");
    check_answers("dict-remove takes a key out", mt_eval(m, E("dict-size", E("dict-remove", mt_keep(prices_space), "pear"))),
                  (int64_t)prices.n);
    check_answers("so it is gone", mt_eval(m, E("dict-has", mt_keep(prices_space), "pear")), B(has(&prices, "pear")));
    removed(&prices, "durian");
    check_answers("removing an absent key is an ordinary answer",
                  mt_eval(m, E("dict-size", E("dict-remove", mt_keep(prices_space), "durian"))), (int64_t)prices.n);
    bool was = removed(&prices, "apple");
    check_answers("dict-remove-pair answers its verdict", mt_eval(m, E("collapse", E("dict-remove-pair", mt_keep(prices_space), "apple"))),
                  was ? E(B(true)) : mt_unit());
    check_answers("the pair is gone", mt_eval(m, E("dict-has", mt_keep(prices_space), "apple")), B(has(&prices, "apple")));
    was = removed(&prices, "apple");
    check_answers("and a second removal finds nothing",
                  mt_eval(m, E("collapse", E("dict-remove-pair", mt_keep(prices_space), "apple"))), was ? E(B(true)) : mt_unit());

    dict stock = { 3, { "apple", "pear", "plum" }, { 12, 12, 4 } };
    mt_atom *stock_space = dict_space(m, &stock);
    check_answers("which keys hold 12", mt_eval(m, E("sort-atom", E("collapse", E("match", mt_keep(stock_space), E(V("k"), 12), V("k"))))),
                  keys_holding(&stock, 12));
    check_answers("dict-get a present key", mt_eval(m, E("dict-get", mt_keep(stock_space), "apple", 0)), get(&stock, "apple", 0));
    check_answers("dict-get an absent one, the default", mt_eval(m, E("dict-get", mt_keep(stock_space), "durian", 0)), get(&stock, "durian", 0));
    update(&stock, "plum", add_six);
    check_answers("dict-update applies the function in place",
                  mt_eval(m, E("dict-get", E("dict-update", mt_keep(stock_space), "plum", E("|->", E(V("v")), E("+", V("v"), 6))), "plum", 0)),
                  get(&stock, "plum", 0));
    update(&stock, "durian", add_six);
    check_answers("and leaves an absent key absent",
                  mt_eval(m, E("dict-get", E("dict-update", mt_keep(stock_space), "durian", E("|->", E(V("v")), E("+", V("v"), 6))),
                               "durian", "absent")),
                  has(&stock, "durian") ? mt_num(get(&stock, "durian", 0)) : mt_sym("absent"));

    dict delivery = { 2, { "pear", "fig" }, { 20, 2 } };
    mt_atom *delivery_space = dict_space(m, &delivery);
    merge(&stock, &delivery);
    check_answers("dict-merge, the second's value winning",
                  mt_eval(m, E("sort-atom", E("dict-pairs", E("dict-merge", mt_keep(stock_space), mt_keep(delivery_space))))), pairs(&stock));
    int64_t fig = get(&stock, "fig", 0);
    removed(&stock, "fig");
    check_answers("dict-pop reads and removes", mt_eval(m, E("dict-pop", mt_keep(stock_space), "fig")), fig);
    check_answers("so the key is gone", mt_eval(m, E("dict-has", mt_keep(stock_space), "fig")), B(has(&stock, "fig")));
    check_answers("and a second pop answers nothing", mt_eval(m, E("collapse", E("dict-pop", mt_keep(stock_space), "fig"))),
                  lookup(&stock, "fig"));
    mt_drop(prices_space);
    mt_drop(stock_space);
    mt_drop(delivery_space);
    return done(m);
}
