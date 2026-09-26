/* Purpose: lib_dict's dictionaries, each a space of (key value) atoms, held
 *   against a C map: keys and values in two arrays, put replacing the value a
 *   key has, remove and pop taking it out, merge putting every pair of one
 *   map into another, update applying a C function to the value in place.
 *   Every operation the engine performs, C performs on its own map, and each
 *   answer must be what the C map says: a size, a membership, a value or the
 *   default, a pattern query's keys, the pairs sorted with mt_order.
 * Guarantees: all twenty-five claims of the original hold
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
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_dict", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_dict")))));

    dict prices = { 2, { "apple", "pear" }, { 3, 5 } };
    mt_atom *prices_space = dict_space(m, &prices);
    assert(answers_are(mt_eval(m, E("dict-size", mt_keep(prices_space))), E((int64_t)prices.n)) && "dict-size is the atom count");
    assert(answers_are(mt_eval(m, E("dict-has", mt_keep(prices_space), "apple")), E(B(has(&prices, "apple")))) && "dict-has a key");
    assert(answers_are(mt_eval(m, E("dict-has", mt_keep(prices_space), "durian")), E(B(has(&prices, "durian")))) && "and one that is not");
    assert(answers_are(mt_eval(m, E("sort-atom", E("collapse", E("dict-values", mt_keep(prices_space))))), E(values(&prices))) && "one value per key");
    assert(answers_are(mt_eval(m, E("sort-atom", E("dict-pairs", mt_keep(prices_space)))), E(pairs(&prices))) && "the pairs");

    put(&prices, "plum", 7);
    assert(answers_are(mt_eval(m, E("dict-size", E("dict-put", mt_keep(prices_space), "plum", 7))), E((int64_t)prices.n))
           && "dict-put writes and answers the dict");
    assert(answers_are(mt_eval(m, E("dict-size", mt_keep(prices_space))), E((int64_t)prices.n)) && "the space itself grew");
    assert(answers_are(mt_eval(m, E("collapse", E("match", mt_keep(prices_space), E("plum", V("v")), V("v")))), E(lookup(&prices, "plum")))
           && "the new key matches");
    put(&prices, "apple", 9);
    assert(answers_are(mt_eval(m, E("dict-size", E("dict-put", mt_keep(prices_space), "apple", 9))), E((int64_t)prices.n))
           && "a key put again is replaced");
    assert(answers_are(mt_eval(m, E("collapse", E("match", mt_keep(prices_space), E("apple", V("v")), V("v")))), E(lookup(&prices, "apple")))
           && "and holds the new value only");

    removed(&prices, "pear");
    assert(answers_are(mt_eval(m, E("dict-size", E("dict-remove", mt_keep(prices_space), "pear"))), E((int64_t)prices.n))
           && "dict-remove takes a key out");
    assert(answers_are(mt_eval(m, E("dict-has", mt_keep(prices_space), "pear")), E(B(has(&prices, "pear")))) && "so it is gone");
    removed(&prices, "durian");
    assert(answers_are(mt_eval(m, E("dict-size", E("dict-remove", mt_keep(prices_space), "durian"))), E((int64_t)prices.n))
           && "removing an absent key is an ordinary answer");
    bool was = removed(&prices, "apple");
    assert(answers_are(mt_eval(m, E("collapse", E("dict-remove-pair", mt_keep(prices_space), "apple"))), E(was ? E(B(true)) : mt_unit()))
           && "dict-remove-pair answers its verdict");
    assert(answers_are(mt_eval(m, E("dict-has", mt_keep(prices_space), "apple")), E(B(has(&prices, "apple")))) && "the pair is gone");
    was = removed(&prices, "apple");
    assert(answers_are(mt_eval(m, E("collapse", E("dict-remove-pair", mt_keep(prices_space), "apple"))), E(was ? E(B(true)) : mt_unit()))
           && "and a second removal finds nothing");

    dict stock = { 3, { "apple", "pear", "plum" }, { 12, 12, 4 } };
    mt_atom *stock_space = dict_space(m, &stock);
    assert(answers_are(mt_eval(m, E("sort-atom", E("collapse", E("match", mt_keep(stock_space), E(V("k"), 12), V("k"))))), E(keys_holding(&stock, 12)))
           && "which keys hold 12");
    assert(answers_are(mt_eval(m, E("dict-get", mt_keep(stock_space), "apple", 0)), E(get(&stock, "apple", 0))) && "dict-get a present key");
    assert(answers_are(mt_eval(m, E("dict-get", mt_keep(stock_space), "durian", 0)), E(get(&stock, "durian", 0))) && "dict-get an absent one, the default");
    update(&stock, "plum", add_six);
    assert(answers_are(mt_eval(m, E("dict-get", E("dict-update", mt_keep(stock_space), "plum", E("|->", E(V("v")), E("+", V("v"), 6))), "plum", 0)), E(get(&stock, "plum", 0)))
           && "dict-update applies the function in place");
    update(&stock, "durian", add_six);
    assert(answers_are(mt_eval(m, E("dict-get", E("dict-update", mt_keep(stock_space), "durian", E("|->", E(V("v")), E("+", V("v"), 6))),
                                    "durian", "absent")), E(has(&stock, "durian") ? mt_num(get(&stock, "durian", 0)) : mt_sym("absent")))
           && "and leaves an absent key absent");

    dict delivery = { 2, { "pear", "fig" }, { 20, 2 } };
    mt_atom *delivery_space = dict_space(m, &delivery);
    merge(&stock, &delivery);
    assert(answers_are(mt_eval(m, E("sort-atom", E("dict-pairs", E("dict-merge", mt_keep(stock_space), mt_keep(delivery_space))))), E(pairs(&stock)))
           && "dict-merge, the second's value winning");
    int64_t fig = get(&stock, "fig", 0);
    removed(&stock, "fig");
    assert(answers_are(mt_eval(m, E("dict-pop", mt_keep(stock_space), "fig")), E(fig)) && "dict-pop reads and removes");
    assert(answers_are(mt_eval(m, E("dict-has", mt_keep(stock_space), "fig")), E(B(has(&stock, "fig")))) && "so the key is gone");
    assert(answers_are(mt_eval(m, E("collapse", E("dict-pop", mt_keep(stock_space), "fig"))), E(lookup(&stock, "fig")))
           && "and a second pop answers nothing");
    mt_drop(prices_space);
    mt_drop(stock_space);
    mt_drop(delivery_space);
    mt_close(m);
    return 0;
}
