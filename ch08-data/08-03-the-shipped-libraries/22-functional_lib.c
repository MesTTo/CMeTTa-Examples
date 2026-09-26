/* Purpose: lib_functional and lib_patrick, held against the same operations
 *   written in C over the children of expressions. A function is a C function
 *   answering any number of values, the way the library applies one: one
 *   answer is an ordinary function, none is (empty), and several are several
 *   equations for one head. An operation that applies one walks its answers
 *   depth first, alternatives in order, so a branching function gives C the
 *   same alternative results, in the same order, as the engine. double, odd?
 *   and grade are C functions the engine calls too, published with mt_def;
 *   the branching heads and the tick counter are equations C builds and adds,
 *   and C's oracle spells each as a C function beside them. A letter is a
 *   codepoint whose general category utf8proc names with an L. Where the
 *   library refuses, C's own operation refuses the same input.
 * Build: cc 22-functional_lib.c $(pkg-config --cflags --libs cmetta
 *   libutf8proc)
 * Assumes: libutf8proc, found through pkg-config.
 * Guarantees: all seventy-three claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#if __has_include(<utf8proc.h>)
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <utf8proc.h>

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

/* The metatype get-metatype answers for an atom of this kind: Symbol for a
   symbol and for a space, which the engine names by a symbol; Variable;
   Expression, the empty one included; Grounded for every value. */
static inline const char *metatype(const mt_atom *atom)
{
    switch (mt_kind_of(atom)) {
    case MT_SYMBOL:
    case MT_SPACE: return "Symbol";
    case MT_VARIABLE: return "Variable";
    case MT_EXPR: return "Expression";
    default: return "Grounded";
    }
}

enum { MOST = 32 };

/* A function over C atoms: its answers for its arguments, written to out. */
typedef size_t function(const mt_atom *const *args, mt_atom **out);

static int64_t at_int(const mt_atom *const *a, size_t i) { return mt_int(a[i]); }

/* The original's own helpers, and the lambdas its claims write. */
static size_t twice(const mt_atom *const *a, mt_atom **out) { return out[0] = mt_num(2 * at_int(a, 0)), 1; }
static size_t odd(const mt_atom *const *a, mt_atom **out) { return out[0] = B(at_int(a, 0) % 2 == 1), 1; }
static size_t grade(const mt_atom *const *a, mt_atom **out) { return out[0] = S(at_int(a, 0) > 50 ? "pass" : "fail"), 1; }
static size_t sum(const mt_atom *const *a, mt_atom **out) { return out[0] = mt_num(at_int(a, 0) + at_int(a, 1)), 1; }
static size_t above_ten(const mt_atom *const *a, mt_atom **out) { return out[0] = B(at_int(a, 0) > 10), 1; }
static size_t second(const mt_atom *const *a, mt_atom **out) { return out[0] = mt_keep(mt_at(a[0], 1)), 1; }
static size_t nothing(const mt_atom *const *a, mt_atom **out) { return (void)a, (void)out, 0; }
static size_t both_ways(const mt_atom *const *a, mt_atom **out) { return (void)a, out[0] = B(false), out[1] = B(true), 2; }
static size_t zero(const mt_atom *const *a, mt_atom **out) { return (void)a, out[0] = mt_num(0), 1; }
static size_t latest(const mt_atom *const *a, mt_atom **out) { return out[0] = mt_keep(a[1]), 1; }
static size_t same(const mt_atom *const *a, mt_atom **out) { return out[0] = mt_keep(a[0]), 1; }
static size_t plus_one(const mt_atom *const *a, mt_atom **out) { return out[0] = mt_num(at_int(a, 0) + 1), 1; }
static size_t plus_two(const mt_atom *const *a, mt_atom **out) { return out[0] = mt_num(at_int(a, 0) + 2), 1; }

static size_t letter(const mt_atom *const *a, mt_atom **out)
{
    utf8proc_int32_t c = -1;
    utf8proc_iterate((const utf8proc_uint8_t *)mt_name(a[0]), (utf8proc_ssize_t)mt_name_len(a[0]), &c);
    return out[0] = B(c >= 0 && utf8proc_category_string(c)[0] == 'L'), 1;
}

/* (cons-atom $x $acc) */
static size_t cons_onto(const mt_atom *const *a, mt_atom **out)
{
    mt_atom *kids[MOST];
    kids[0] = mt_keep(a[1]);
    for (size_t i = 0; i < mt_len(a[0]); i++) kids[i + 1] = mt_keep(mt_at(a[0], i));
    return out[0] = mt_exprv(mt_len(a[0]) + 1, kids), 1;
}

/* (if (< $n 4) ($n (+ $n 1)) (empty)) */
static size_t count_to_four(const mt_atom *const *a, mt_atom **out)
{
    int64_t n = at_int(a, 0);
    return n < 4 ? (out[0] = E(n, n + 1), 1) : 0;
}

/* (if (== $n 0) (1 2 3) (empty)): an answer that is no (Value NextSeed). */
static size_t three_wide(const mt_atom *const *a, mt_atom **out)
{
    return at_int(a, 0) == 0 ? (out[0] = E(1, 2, 3), 1) : 0;
}

static size_t is_expression(const mt_atom *const *a, mt_atom **out) { return out[0] = B(mt_kind_of(a[0]) == MT_EXPR), 1; }

static size_t metatype_of(const mt_atom *const *a, mt_atom **out) { return out[0] = S(metatype(a[0])), 1; }

/* The branching heads, two equations each. */
static size_t branch_add(const mt_atom *const *a, mt_atom **out)
{
    int64_t s = at_int(a, 0) + at_int(a, 1);
    return out[0] = mt_num(s), out[1] = mt_num(1 + s), 2;
}

static size_t branch_key(const mt_atom *const *a, mt_atom **out)
{
    int64_t r = at_int(a, 0) % 2;
    return out[0] = mt_num(r), out[1] = mt_num(2 + r), 2;
}

static size_t branch_step(const mt_atom *const *a, mt_atom **out)
{
    int64_t seed = at_int(a, 0);
    return seed < 2 ? (out[0] = E(seed, seed + 1), out[1] = E(seed + 10, seed + 1), 2) : 0;
}

/* A function the engine calls: C answers the one value it computes. */
typedef struct published {
    function *fn;
    size_t arity;
} published;

static mt_status applied(mt_call *call, void *user)
{
    const published *p = user;
    const mt_atom *args[2];
    mt_atom *out[MOST];
    for (size_t i = 0; i < p->arity; i++) args[i] = mt_arg(call, i);
    mt_clear();
    size_t n = p->fn(args, out);
    if (!mt_ok() || n != 1) {
        while (n) mt_drop(out[--n]);
        return mt_fail(call, "wants integers");
    }
    return mt_answer(call, out[0]);
}

/* The operations. */
static mt_atom *zipped(const mt_atom *a, const mt_atom *b)
{
    mt_atom *pairs[MOST];
    size_t n = mt_len(a) < mt_len(b) ? mt_len(a) : mt_len(b);
    for (size_t i = 0; i < n; i++) pairs[i] = E(mt_keep(mt_at(a, i)), mt_keep(mt_at(b, i)));
    return mt_exprv(n, pairs);
}

/* NULL when an element is no two-element expression. */
static mt_atom *unzipped(const mt_atom *pairs)
{
    mt_atom *left[MOST], *right[MOST];
    size_t n = mt_len(pairs);
    for (size_t i = 0; i < n; i++)
        if (mt_kind_of(mt_at(pairs, i)) != MT_EXPR || mt_len(mt_at(pairs, i)) != 2) {
            while (i) mt_drop(left[--i]), mt_drop(right[i]);
            return NULL;
        } else
            left[i] = mt_keep(mt_at(mt_at(pairs, i), 0)), right[i] = mt_keep(mt_at(mt_at(pairs, i), 1));
    return E(mt_exprv(n, left), mt_exprv(n, right));
}

static mt_atom *slice(const mt_atom *xs, size_t from, size_t to)
{
    mt_atom *kids[MOST];
    for (size_t i = from; i < to; i++) kids[i - from] = mt_keep(mt_at(xs, i));
    return mt_exprv(to - from, kids);
}

static mt_atom *dropped(const mt_atom *xs, int64_t k) { return slice(xs, (size_t)k < mt_len(xs) ? (size_t)k : mt_len(xs), mt_len(xs)); }

/* Pieces of width elements, each starting step after the last: chunk cuts
   (step = width, a short last piece kept) and window slides (step 1, whole
   pieces only). NULL for a width below one, which would never finish. */
static mt_atom *pieces(const mt_atom *xs, int64_t width, int64_t step, bool partial)
{
    if (width < 1) return NULL;
    mt_atom *out[MOST];
    size_t n = 0, len = mt_len(xs), w = (size_t)width;
    for (size_t from = 0; from < len && (partial || from + w <= len); from += (size_t)step)
        out[n++] = slice(xs, from, from + w < len ? from + w : len);
    return mt_exprv(n, out);
}

static void flatten_into(const mt_atom *xs, bool deep, mt_atom **out, size_t *n)
{
    for (size_t i = 0; i < mt_len(xs); i++) {
        const mt_atom *x = mt_at(xs, i);
        if (mt_kind_of(x) != MT_EXPR) out[(*n)++] = mt_keep(x);
        else if (deep) flatten_into(x, true, out, n);
        else
            for (size_t j = 0; j < mt_len(x); j++) out[(*n)++] = mt_keep(mt_at(x, j));
    }
}

static mt_atom *flattened(const mt_atom *xs, bool deep)
{
    mt_atom *out[MOST];
    size_t n = 0;
    flatten_into(xs, deep, out, &n);
    return mt_exprv(n, out);
}

/* An item goes Yes when some answer of the test is True. */
static mt_atom *partitioned(function *test, const mt_atom *xs)
{
    mt_atom *yes[MOST], *no[MOST], *verdicts[MOST];
    size_t y = 0, n = 0;
    for (size_t i = 0; i < mt_len(xs); i++) {
        const mt_atom *x = mt_at(xs, i);
        size_t v = test(&x, verdicts);
        bool some = false;
        while (v) {
            mt_atom *verdict = verdicts[--v];
            some |= mt_kind_of(verdict) == MT_BOOL && mt_truth(verdict);
            mt_drop(verdict);
        }
        if (some) yes[y++] = mt_keep(x);
        else no[n++] = mt_keep(x);
    }
    return E(mt_exprv(y, yes), mt_exprv(n, no));
}

/* The groups one choice of keys makes: keys in first-appearance order, each
   with its members in the collection's order. */
static mt_atom *groups(const mt_atom *xs, const mt_atom *const *keys)
{
    mt_atom *out[MOST];
    size_t first[MOST], n = 0;
    for (size_t i = 0; i < mt_len(xs); i++) {
        size_t g = 0;
        while (g < n && !mt_eq(keys[first[g]], keys[i])) g++;
        if (g == n) first[n++] = i;
    }
    for (size_t g = 0; g < n; g++) {
        mt_atom *members[MOST];
        size_t m = 0;
        for (size_t i = 0; i < mt_len(xs); i++)
            if (mt_eq(keys[first[g]], keys[i])) members[m++] = mt_keep(mt_at(xs, i));
        out[g] = E(mt_keep(keys[first[g]]), mt_exprv(m, members));
    }
    return mt_exprv(n, out);
}

/* Every grouping, one per choice of each item's key among its answers,
   depth first. Time: one groups() per leaf of the answer tree. */
static void group_from(function *key, const mt_atom *xs, const mt_atom **keys, size_t at, mt_atom **out, size_t *found)
{
    if (at == mt_len(xs)) {
        out[(*found)++] = groups(xs, keys);
        return;
    }
    mt_atom *answers[MOST];
    const mt_atom *x = mt_at(xs, at);
    size_t n = key(&x, answers);
    for (size_t i = 0; i < n; i++) {
        keys[at] = answers[i];
        group_from(key, xs, keys, at + 1, out, found);
    }
    while (n) mt_drop(answers[--n]);
}

static mt_atom *grouped(function *key, const mt_atom *xs)
{
    mt_atom *out[MOST];
    const mt_atom *keys[MOST];
    size_t n = 0;
    group_from(key, xs, keys, 0, out, &n);
    return mt_exprv(n, out);
}

static mt_atom *only(mt_atom *alternatives)
{
    require("one alternative", mt_len(alternatives) == 1);
    mt_atom *one = mt_keep(mt_at(alternatives, 0));
    mt_drop(alternatives);
    return one;
}

/* The groups sorted by term order, their members concatenated: a stable
   sort, since each group keeps its members in order. */
static mt_atom *sorted_by(function *key, const mt_atom *xs)
{
    mt_atom *by_key = only(grouped(key, xs)), *each[MOST], *out[MOST];
    size_t n = mt_len(by_key), total = 0;
    for (size_t g = 0; g < n; g++) each[g] = mt_keep(mt_at(by_key, g));
    qsort(each, n, sizeof *each, mt_order);
    for (size_t g = 0; g < n; g++) {
        for (size_t i = 0; i < mt_len(mt_at(each[g], 1)); i++) out[total++] = mt_keep(mt_at(mt_at(each[g], 1), i));
        mt_drop(each[g]);
    }
    mt_drop(by_key);
    return mt_exprv(total, out);
}

static mt_atom *kept(const mt_atom *const *at, size_t n)
{
    mt_atom *kids[MOST];
    for (size_t i = 0; i < n; i++) kids[i] = mt_keep(at[i]);
    return mt_exprv(n, kids);
}

/* Every history of a fold, the start first: each answer extends a history
   and a function answering nothing ends it. */
static void scan_from(function *f, const mt_atom **history, size_t len, const mt_atom *xs, mt_atom **out, size_t *found)
{
    if (len - 1 == mt_len(xs)) {
        out[(*found)++] = kept(history, len);
        return;
    }
    mt_atom *answers[MOST];
    const mt_atom *args[] = { history[len - 1], mt_at(xs, len - 1) };
    size_t n = f(args, answers);
    for (size_t i = 0; i < n; i++) {
        history[len] = answers[i];
        scan_from(f, history, len + 1, xs, out, found);
    }
    while (n) mt_drop(answers[--n]);
}

static mt_atom *scanned(function *f, const mt_atom *start, const mt_atom *xs)
{
    mt_atom *out[MOST];
    const mt_atom *history[MOST] = { start };
    size_t n = 0;
    scan_from(f, history, 1, xs, out, &n);
    return mt_exprv(n, out);
}

/* Every collection a seed grows into: each answer must be (Value
   NextSeed), and none ends that path. False on a malformed answer. */
static bool unfold_from(function *step, const mt_atom *seed, const mt_atom **values, size_t len, mt_atom **out, size_t *found)
{
    mt_atom *answers[MOST];
    size_t n = step(&seed, answers);
    bool ok = true;
    if (!n) out[(*found)++] = kept(values, len);
    for (size_t i = 0; i < n && ok; i++) {
        ok = mt_kind_of(answers[i]) == MT_EXPR && mt_len(answers[i]) == 2;
        if (ok) {
            values[len] = mt_at(answers[i], 0);
            ok = unfold_from(step, mt_at(answers[i], 1), values, len + 1, out, found);
        }
    }
    while (n) mt_drop(answers[--n]);
    return ok;
}

static mt_atom *unfolded(function *step, const mt_atom *seed)
{
    mt_atom *out[MOST];
    const mt_atom *values[MOST];
    size_t n = 0;
    if (unfold_from(step, seed, values, 0, out, &n)) return mt_exprv(n, out);
    while (n) mt_drop(out[--n]);
    return NULL;
}

/* A stage of a pipe: the functions its entry evaluates to. */
typedef struct stage {
    function *const *each;
    size_t n;
} stage;

static void pipe_from(const stage *stages, size_t count, const mt_atom *x, mt_atom **out, size_t *found)
{
    if (!count) {
        out[(*found)++] = mt_keep(x);
        return;
    }
    for (size_t f = 0; f < stages[0].n; f++) {
        mt_atom *answers[MOST];
        size_t n = stages[0].each[f](&x, answers);
        for (size_t i = 0; i < n; i++) pipe_from(stages + 1, count - 1, answers[i], out, found);
        while (n) mt_drop(answers[--n]);
    }
}

static mt_atom *piped(const stage *stages, size_t count, const mt_atom *x)
{
    mt_atom *out[MOST];
    size_t n = 0;
    pipe_from(stages, count, x, out, &n);
    return mt_exprv(n, out);
}

static mt_atom *applied_to(function *f, const mt_atom *args)
{
    mt_atom *out[MOST];
    const mt_atom *each[MOST];
    for (size_t i = 0; i < mt_len(args); i++) each[i] = mt_at(args, i);
    return mt_exprv(f(each, out), out);
}

/* repeat: the body's value, once per iteration. */
static size_t repeated(int64_t times, int64_t value, mt_atom **out)
{
    size_t n = 0;
    for (int64_t i = 0; i < times; i++) out[n++] = mt_num(value);
    return n;
}

/* unless: the body's value when the condition is False, else nothing. TAKES
   body. */
static size_t unless_(bool condition, mt_atom *body, mt_atom **out)
{
    if (condition) return mt_drop(body), 0;
    return out[0] = body, 1;
}

static mt_atom *verdict(bool holds) { return S(holds ? "fine" : "refused"); }
static mt_atom *guarded(mt_atom *goal) { return E("if-error", E("catch", goal), "refused", "fine"); }

/* A value C computed, which is refused when it is NULL; TAKES it. */
static bool computed(mt_atom *value)
{
    mt_drop(value);
    return value != NULL;
}

static mt_atom *lambda(mt_atom *params, mt_atom *body) { return E("|->", params, body); }
static mt_atom *collapsed(mt_atom *goal) { return E("collapse", goal); }

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_functional", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_functional")))));
    require("import lib_unicode", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_unicode")))));
    static published doubled = { twice, 1 }, oddness = { odd, 1 }, graded = { grade, 1 };
    require("double", mt_def(m, (mt_op){ .name = "double", .arity = 1, .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = applied, .user = &doubled }));
    require("odd?", mt_def(m, (mt_op){ .name = "odd?", .arity = 1, .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = applied, .user = &oddness }));
    require("grade", mt_def(m, (mt_op){ .name = "grade", .arity = 1, .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = applied, .user = &graded }));

    /* zip stops at the shorter collection; unzip inverts it. */
    mt_atom *none = mt_unit(), *just_a = E("a");
    mt_atom *nums = E(1, 2, 3), *abc = E("a", "b", "c"), *ab = E("a", "b"), *pairs = E(E(1, "a"), E(2, "b")), *one_two = E(1, 2);
    assert(answers_are(mt_eval(m, E("zip", mt_keep(nums), mt_keep(abc))), E(zipped(nums, abc))) && "zip");
    assert(answers_are(mt_eval(m, E("zip", mt_keep(nums), mt_keep(ab))), E(zipped(nums, ab))) && "zip stops at the shorter");
    assert(answers_are(mt_eval(m, E("zip", mt_unit(), mt_keep(just_a))), E(zipped(none, just_a))) && "zip with nothing");
    assert(answers_are(mt_eval(m, E("unzip", mt_keep(pairs))), E(unzipped(pairs))) && "unzip");
    mt_atom *rezipped = zipped(one_two, ab);
    assert(answers_are(mt_eval(m, E("unzip", E("zip", mt_keep(one_two), mt_keep(ab)))), E(unzipped(rezipped))) && "unzip inverts zip");
    mt_drop(rezipped);
    mt_atom *lone = E(1);
    assert(answers_are(mt_eval(m, guarded(E("unzip", mt_keep(lone)))), E(verdict(computed(unzipped(lone))))) && "unzip wants pairs");

    mt_atom *four = E(1, 2, 3, 4), *five = E(1, 2, 3, 4, 5);
    assert(answers_are(mt_eval(m, E("drop", mt_keep(four), 2)), E(dropped(four, 2))) && "drop");
    assert(answers_are(mt_eval(m, E("drop", mt_keep(one_two), 5)), E(dropped(one_two, 5))) && "dropping more than there is");
    assert(answers_are(mt_eval(m, E("drop", mt_keep(one_two), 0)), E(dropped(one_two, 0))) && "dropping none");

    /* chunk cuts, window slides. */
    assert(answers_are(mt_eval(m, E("chunk", mt_keep(five), 2)), E(pieces(five, 2, 2, true))) && "chunk");
    assert(answers_are(mt_eval(m, E("chunk", mt_keep(four), 2)), E(pieces(four, 2, 2, true))) && "chunk evenly");
    assert(answers_are(mt_eval(m, E("chunk", mt_unit(), 2)), E(pieces(none, 2, 2, true))) && "no chunks of nothing");
    assert(answers_are(mt_eval(m, E("window", mt_keep(four), 2)), E(pieces(four, 2, 1, false))) && "window");
    assert(answers_are(mt_eval(m, E("window", mt_keep(nums), 3)), E(pieces(nums, 3, 1, false))) && "one whole window");
    assert(answers_are(mt_eval(m, E("window", mt_keep(one_two), 3)), E(pieces(one_two, 3, 1, false))) && "no window wider than the collection");
    assert(answers_are(mt_eval(m, guarded(E("chunk", mt_keep(one_two), 0))), E(verdict(computed(pieces(one_two, 0, 0, true))))) && "chunk wants a width");
    assert(answers_are(mt_eval(m, guarded(E("window", mt_keep(one_two), 0))), E(verdict(computed(pieces(one_two, 0, 1, false))))) && "window wants a width");

    /* One level, or every level. */
    mt_atom *nested = E(E(1, 2), E(3), mt_unit(), 4), *inner = E(E(E(1, E(2))), 3), *deep = E(E(1, E(2, E(3))), 4),
            *hollow = E(E(E(mt_unit())));
    assert(answers_are(mt_eval(m, E("flatten-once", mt_keep(nested))), E(flattened(nested, false))) && "flatten-once");
    assert(answers_are(mt_eval(m, E("flatten-once", mt_keep(inner))), E(flattened(inner, false))) && "one level leaves the rest as data");
    assert(answers_are(mt_eval(m, E("flatten-deep", mt_keep(deep))), E(flattened(deep, true))) && "flatten-deep");
    assert(answers_are(mt_eval(m, E("flatten-deep", mt_keep(hollow))), E(flattened(hollow, true))) && "empty collections contribute nothing");

    /* partition reads the verdict. */
    assert(answers_are(mt_eval(m, E("partition", "odd?", mt_keep(four))), E(partitioned(odd, four))) && "partition");
    assert(answers_are(mt_eval(m, E("partition", "odd?", mt_unit())), E(partitioned(odd, none))) && "partition of nothing");
    assert(answers_are(mt_eval(m, E("partition", lambda(E(V("x")), E(">", V("x"), 10)), mt_keep(one_two))), E(partitioned(above_ten, one_two)))
           && "a lambda test");
    mt_atom *chars = E(T("a"), T("1"));
    assert(answers_are(mt_eval(m, E("partition", lambda(E(V("c")), E("unicode-is", V("c"), "letter")), mt_keep(chars))), E(partitioned(letter, chars)))
           && "a deterministic head's False is read");

    /* group-by gathers in first-appearance order. */
    mt_atom *scores = E(80, 20, 90), *shuffled = E(3, 1, 2), *rows = E(E("b", 1), E("a", 1), E("c", 0));
    assert(answers_are(mt_eval(m, E("group-by", "odd?", mt_keep(four))), E(only(grouped(odd, four)))) && "group-by");
    assert(answers_are(mt_eval(m, E("group-by", "grade", mt_keep(scores))), E(only(grouped(grade, scores)))) && "group-by grade");
    assert(answers_are(mt_eval(m, E("group-by", "odd?", mt_unit())), E(only(grouped(odd, none)))) && "no groups of nothing");
    assert(answers_are(mt_eval(m, E("sort-by", "double", mt_keep(shuffled))), E(sorted_by(twice, shuffled))) && "sort-by");
    assert(answers_are(mt_eval(m, E("sort-by", lambda(E(V("p")), E("index-atom", V("p"), 1)), mt_keep(rows))), E(sorted_by(second, rows)))
           && "a stable sort");

    /* scan answers the running results; unfold grows a seed. */
    mt_atom *start = mt_num(0);
    assert(answers_are(mt_eval(m, E("scan", "+", 0, mt_keep(nums))), E(only(scanned(sum, start, nums)))) && "scan");
    assert(answers_are(mt_eval(m, E("scan", "+", 0, mt_unit())), E(only(scanned(sum, start, none)))) && "scan of nothing is the start");
    assert(answers_are(mt_eval(m, E("scan", lambda(E(V("acc"), V("x")), E("cons-atom", V("x"), V("acc"))), mt_unit(), mt_keep(one_two))), E(only(scanned(cons_onto, none, one_two))))
           && "scan with a lambda");
    mt_atom *first_seed = mt_num(1);
    assert(answers_are(mt_eval(m, E("unfold", lambda(E(V("n")), E("if", E("<", V("n"), 4), E(V("n"), E("+", V("n"), 1)), E("empty"))), 1)), E(only(unfolded(count_to_four, first_seed))))
           && "unfold");
    assert(answers_are(mt_eval(m, E("unfold", lambda(E(V("n")), E("empty")), 1)), E(only(unfolded(nothing, first_seed))))
           && "a step answering nothing");

    /* pipe reads left to right; apply-to makes a call of a collection. */
    function *const just_double[] = { twice }, *const just_odd[] = { odd }, *const increments[] = { plus_one, plus_two },
                    *const identity[] = { same };
    const stage twice_twice[] = { { just_double, 1 }, { just_double, 1 } }, twice_odd[] = { { just_double, 1 }, { just_odd, 1 } };
    mt_atom *three = mt_num(3);
    assert(answers_are(mt_eval(m, E("pipe", E("double", "double"), 3)), E(only(piped(twice_twice, 2, three)))) && "pipe");
    assert(answers_are(mt_eval(m, E("pipe", mt_unit(), 3)), E(only(piped(NULL, 0, three)))) && "an empty pipe");
    assert(answers_are(mt_eval(m, E("pipe", E("double", "odd?"), 3)), E(only(piped(twice_odd, 2, three)))) && "a pipe into a test");
    mt_atom *five_only = E(5);
    assert(answers_are(mt_eval(m, E("apply-to", "+", mt_keep(one_two))), E(only(applied_to(sum, one_two)))) && "apply-to");
    assert(answers_are(mt_eval(m, E("apply-to", "double", mt_keep(five_only))), E(only(applied_to(twice, five_only)))) && "apply-to one argument");

    /* The control forms, beside C's own loop and if. */
    mt_atom *out[MOST];
    assert(answers_are(mt_eval(m, E("repeat", 3, E("+", 3, 4))), mt_exprv(repeated(3, 3 + 4, out), out)) && "repeat");
    assert(answers_are(mt_eval(m, E("repeat", 0, E("+", 3, 4))), mt_exprv(repeated(0, 3 + 4, out), out)) && "repeat zero times");
    assert(answers_are(mt_eval(m, E("unless", B(false), E("+", 1, 1))), mt_exprv(unless_(false, mt_num(1 + 1), out), out)) && "unless False");
    assert(answers_are(mt_eval(m, E("unless", B(true), E("+", 1, 1))), mt_exprv(unless_(true, mt_num(1 + 1), out), out)) && "unless True");
    assert(!mt_first(mt_eval(m, E("while", B(false), "never"))) && mt_ok() && "while False runs its body no times");

    /* A while loop over a counter the space holds, beside C's over its own. */
    mt_space *ticks = mt_space_open(m, "&ticks");
    require("&ticks", ticks != NULL && mt_add(ticks, E("count", 0)));
    require("ticks", mt_add(m, E("=", E("ticks"), E("car-atom", E("collapse", E("match", "&ticks", E("count", V("n")), V("n")))))));
    require("tick", mt_add(m, E("=", E("tick"),
                                E("let", V("now"), E("ticks"),
                                  E("let", V("gone"), E("remove-atom", "&ticks", E("count", V("now"))),
                                    E("let", V("next"), E("+", V("now"), 1),
                                      E("let", V("added"), E("add-atom", "&ticks", E("count", V("next"))), V("next"))))))));
    int64_t counter = 0, ticked[MOST];
    size_t loops = 0;
    while (counter < 3) ticked[loops++] = ++counter;
    mt_atom *each_tick[MOST];
    for (size_t i = 0; i < loops; i++) each_tick[i] = mt_num(ticked[i]);
    assert(answers_are(mt_eval(m, E("while", E("<", E("ticks"), 3), E("tick"))), mt_exprv(loops, each_tick)) && "while advances the counter");
    assert(answers_are(mt_eval(m, E("ticks")), E(counter)) && "and leaves it at the bound");
    mt_space_close(ticks);

    /* lib_patrick's four. compose reads right to left. */
    require("import lib_patrick", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_patrick")))));
    const stage odd_after_double[] = { { just_double, 1 }, { just_odd, 1 } };
    assert(answers_are(mt_eval(m, E("compose", E("double", "double"), E(3))), E(only(piped(twice_twice, 2, three)))) && "compose");
    assert(answers_are(mt_eval(m, E("compose", E("odd?", "double"), E(3))), E(only(piped(odd_after_double, 2, three)))) && "compose right to left");
    int64_t bound = 1 + 2;
    assert(answers_are(mt_eval(m, E("@", V("x"), E("+", 1, 2))), E(bound)) && "@ binds and answers");
    int64_t tens[3];
    for (int64_t x = 1; x <= 3; x++) tens[x - 1] = x * 10;
    assert(answers_are(mt_eval(m, E("for", V("x"), mt_keep(nums), E("*", V("x"), 10))), E(tens[0], tens[1], tens[2])) && "for");
    int64_t state = 0;
    for (int64_t i = 0; i < 3; i++) state += i;
    assert(answers_are(mt_eval(m, E("iterate", 0, 3, 0, lambda(E(V("i"), V("s")), E("+", V("s"), V("i"))))), E(state)) && "iterate");

    /* Branching functions: every answer stays a path. */
    require("branch-add", mt_add(m, E("=", E("branch-add", V("a"), V("b")), E("+", V("a"), V("b")))) &&
                          mt_add(m, E("=", E("branch-add", V("a"), V("b")), E("+", 1, E("+", V("a"), V("b"))))));
    require("branch-key", mt_add(m, E("=", E("branch-key", V("x")), E("%", V("x"), 2))) &&
                          mt_add(m, E("=", E("branch-key", V("x")), E("+", 2, E("%", V("x"), 2)))));
    require("branch-step", mt_add(m, E("=", E("branch-step", V("seed")),
                                       E("if", E("<", V("seed"), 2), E(V("seed"), E("+", V("seed"), 1)), E("empty")))) &&
                           mt_add(m, E("=", E("branch-step", V("seed")),
                                       E("if", E("<", V("seed"), 2), E(E("+", V("seed"), 10), E("+", V("seed"), 1)), E("empty")))));
    require("increment-function", mt_add(m, E("=", E("increment-function"), lambda(E(V("x")), E("+", V("x"), 1)))) &&
                                  mt_add(m, E("=", E("increment-function"), lambda(E(V("x")), E("+", V("x"), 2)))));
    mt_atom *zero_one = E(0, 1), *seed = mt_num(0);
    assert(answers_are(mt_eval(m, collapsed(E("scan", "branch-add", 0, mt_keep(one_two)))), E(scanned(branch_add, start, one_two))) && "a branching fold");
    assert(answers_are(mt_eval(m, collapsed(E("unfold", "branch-step", 0))), E(unfolded(branch_step, seed))) && "a branching unfold");
    assert(answers_are(mt_eval(m, collapsed(E("group-by", "branch-key", mt_keep(zero_one)))), E(grouped(branch_key, zero_one))) && "a branching key");
    const stage branching[] = { { increments, 2 }, { increments, 2 } };
    assert(answers_are(mt_eval(m, collapsed(E("pipe", E(E("increment-function"), E("increment-function")), 0))), E(piped(branching, 2, seed)))
           && "a branching pipe");
    mt_atom *one_arg = E(1), *incremented[4];
    size_t k = 0;
    for (size_t f = 0; f < 2; f++) {
        mt_atom *answers = applied_to(increments[f], one_arg);
        for (size_t i = 0; i < mt_len(answers); i++) incremented[k++] = mt_keep(mt_at(answers, i));
        mt_drop(answers);
    }
    assert(answers_are(mt_eval(m, collapsed(E("apply-to", E("increment-function"), mt_keep(one_arg)))), E(mt_exprv(k, incremented)))
           && "apply-to a branching function");
    assert(answers_are(mt_eval(m, E("partition", lambda(E(V("x")), E("superpose", E(B(false), B(true)))), mt_keep(one_two))), E(partitioned(both_ways, one_two)))
           && "some True answer is Yes");
    assert(answers_are(mt_eval(m, E("partition", lambda(E(V("x")), E("empty")), mt_keep(one_two))), E(partitioned(nothing, one_two))) && "no answer is No");
    assert(answers_are(mt_eval(m, collapsed(E("scan", lambda(E(V("a"), V("b")), E("empty")), 0, E(1)))), E(scanned(nothing, start, one_arg)))
           && "a fold with no answer has no history");
    assert(answers_are(mt_eval(m, guarded(E("unfold", lambda(E(V("n")), E("if", E("==", V("n"), 0), E(1, 2, 3), E("empty"))), 0))), E(verdict(computed(unfolded(three_wide, seed)))))
           && "a step must answer (Value NextSeed)");

    /* Literal values, Error-headed data among them, stay data. */
    mt_atom *arith = E("+", 1, 2), *error = E("Error", "a", "b"), *literal = E(mt_keep(arith), mt_keep(error)),
            *heads = E(E("+", "a"), E("Error", "b")), *wrapped = E(E(mt_keep(arith)), mt_keep(error)),
            *with_three = E(mt_keep(arith), mt_keep(error), 3), *just_arith = E(mt_keep(arith)), *a = S("a");
    assert(answers_are(mt_eval(m, E("zip", E("quote", mt_keep(literal)), mt_keep(ab))), E(zipped(literal, ab))) && "zip keeps data");
    assert(answers_are(mt_eval(m, E("unzip", E("quote", mt_keep(heads)))), E(unzipped(heads))) && "unzip keeps data");
    assert(answers_are(mt_eval(m, E("flatten-once", E("quote", mt_keep(wrapped)))), E(flattened(wrapped, false))) && "flatten-once keeps data");
    assert(answers_are(mt_eval(m, E("flatten-deep", E("quote", mt_keep(wrapped)))), E(flattened(wrapped, true))) && "flatten-deep keeps data");
    assert(answers_are(mt_eval(m, E("partition", lambda(E(V("x")), E("==", E("get-metatype", V("x")), "Expression")), E("quote", mt_keep(with_three)))), E(partitioned(is_expression, with_three)))
           && "partition keeps data");
    assert(answers_are(mt_eval(m, E("group-by", lambda(E(V("x")), E("get-metatype", V("x"))), E("quote", mt_keep(literal)))), E(only(grouped(metatype_of, literal))))
           && "group-by keeps data");
    assert(answers_are(mt_eval(m, E("sort-by", lambda(E(V("x")), mt_num(0)), E("quote", mt_keep(literal)))), E(sorted_by(zero, literal))) && "sort-by keeps data");
    assert(answers_are(mt_eval(m, E("scan", lambda(E(V("acc"), V("item")), E("quote", V("item"))), "a", E("quote", mt_keep(literal)))), E(only(scanned(latest, a, literal))))
           && "scan keeps data");
    assert(answers_are(mt_eval(m, E("chunk", E("quote", mt_keep(literal)), 1)), E(pieces(literal, 1, 1, true))) && "chunk keeps data");
    assert(answers_are(mt_eval(m, E("window", E("quote", mt_keep(literal)), 2)), E(pieces(literal, 2, 1, false))) && "window keeps data");
    assert(answers_are(mt_eval(m, E("apply-to", lambda(E(V("data")), E("quote", V("data"))), E("quote", mt_keep(just_arith)))), E(only(applied_to(same, just_arith))))
           && "apply-to keeps data");
    const stage identity_stage[] = { { identity, 1 } };
    assert(answers_are(mt_eval(m, E("pipe", E(lambda(E(V("data")), E("quote", V("data")))), E("quote", mt_keep(arith)))), E(only(piped(identity_stage, 1, arith))))
           && "pipe keeps data");

    mt_atom *held[] = { none, just_a, nums, abc, ab, pairs, one_two, lone, four, five, nested, inner, deep, hollow, chars, scores, shuffled, rows,
                        start, first_seed, three, five_only, zero_one, seed, one_arg, arith, error, literal, heads, wrapped,
                        with_three, just_arith, a };
    for (size_t i = 0; i < sizeof held / sizeof *held; i++) mt_drop(held[i]);
    mt_close(m);
    return 0;
}
#else
#include <stdio.h>

/* Without utf8proc's headers the program only says what it needs. */
int main(void)
{
    fputs("22-functional_lib.c needs utf8proc: install its development files, then build with\n"
          "cc 22-functional_lib.c $(pkg-config --cflags --libs cmetta libutf8proc)\n", stderr);
    return 77;
}
#endif
