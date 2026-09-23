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
 * Assumes: libutf8proc, found through pkg-config.
 * Guarantees: all seventy-three claims of the original hold [tested: make
 *   twins; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"
#include <utf8proc.h>

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

static size_t metatype(const mt_atom *const *a, mt_atom **out)
{
    mt_kind k = mt_kind_of(a[0]);
    return out[0] = S(k == MT_EXPR ? "Expression" : k == MT_SYMBOL ? "Symbol" : k == MT_VARIABLE ? "Variable" : "Grounded"), 1;
}

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
    metta *m = open_engine();
    require("import lib_functional", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_functional")))));
    require("import lib_unicode", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_unicode")))));
    static published doubled = { twice, 1 }, oddness = { odd, 1 }, graded = { grade, 1 };
    require("double", mt_def(m, (mt_op){ .name = "double", .arity = 1, .effect = MT_PURE, .fn = applied, .user = &doubled }));
    require("odd?", mt_def(m, (mt_op){ .name = "odd?", .arity = 1, .effect = MT_PURE, .fn = applied, .user = &oddness }));
    require("grade", mt_def(m, (mt_op){ .name = "grade", .arity = 1, .effect = MT_PURE, .fn = applied, .user = &graded }));

    /* zip stops at the shorter collection; unzip inverts it. */
    mt_atom *none = mt_unit(), *just_a = E("a");
    mt_atom *nums = E(1, 2, 3), *abc = E("a", "b", "c"), *ab = E("a", "b"), *pairs = E(E(1, "a"), E(2, "b")), *one_two = E(1, 2);
    check_answers("zip", mt_eval(m, E("zip", mt_keep(nums), mt_keep(abc))), zipped(nums, abc));
    check_answers("zip stops at the shorter", mt_eval(m, E("zip", mt_keep(nums), mt_keep(ab))), zipped(nums, ab));
    check_answers("zip with nothing", mt_eval(m, E("zip", mt_unit(), mt_keep(just_a))), zipped(none, just_a));
    check_answers("unzip", mt_eval(m, E("unzip", mt_keep(pairs))), unzipped(pairs));
    mt_atom *rezipped = zipped(one_two, ab);
    check_answers("unzip inverts zip", mt_eval(m, E("unzip", E("zip", mt_keep(one_two), mt_keep(ab)))), unzipped(rezipped));
    mt_drop(rezipped);
    mt_atom *lone = E(1);
    check_answers("unzip wants pairs", mt_eval(m, guarded(E("unzip", mt_keep(lone)))), verdict(computed(unzipped(lone))));

    mt_atom *four = E(1, 2, 3, 4), *five = E(1, 2, 3, 4, 5);
    check_answers("drop", mt_eval(m, E("drop", mt_keep(four), 2)), dropped(four, 2));
    check_answers("dropping more than there is", mt_eval(m, E("drop", mt_keep(one_two), 5)), dropped(one_two, 5));
    check_answers("dropping none", mt_eval(m, E("drop", mt_keep(one_two), 0)), dropped(one_two, 0));

    /* chunk cuts, window slides. */
    check_answers("chunk", mt_eval(m, E("chunk", mt_keep(five), 2)), pieces(five, 2, 2, true));
    check_answers("chunk evenly", mt_eval(m, E("chunk", mt_keep(four), 2)), pieces(four, 2, 2, true));
    check_answers("no chunks of nothing", mt_eval(m, E("chunk", mt_unit(), 2)), pieces(none, 2, 2, true));
    check_answers("window", mt_eval(m, E("window", mt_keep(four), 2)), pieces(four, 2, 1, false));
    check_answers("one whole window", mt_eval(m, E("window", mt_keep(nums), 3)), pieces(nums, 3, 1, false));
    check_answers("no window wider than the collection", mt_eval(m, E("window", mt_keep(one_two), 3)), pieces(one_two, 3, 1, false));
    check_answers("chunk wants a width", mt_eval(m, guarded(E("chunk", mt_keep(one_two), 0))), verdict(computed(pieces(one_two, 0, 0, true))));
    check_answers("window wants a width", mt_eval(m, guarded(E("window", mt_keep(one_two), 0))), verdict(computed(pieces(one_two, 0, 1, false))));

    /* One level, or every level. */
    mt_atom *nested = E(E(1, 2), E(3), mt_unit(), 4), *inner = E(E(E(1, E(2))), 3), *deep = E(E(1, E(2, E(3))), 4),
            *hollow = E(E(E(mt_unit())));
    check_answers("flatten-once", mt_eval(m, E("flatten-once", mt_keep(nested))), flattened(nested, false));
    check_answers("one level leaves the rest as data", mt_eval(m, E("flatten-once", mt_keep(inner))), flattened(inner, false));
    check_answers("flatten-deep", mt_eval(m, E("flatten-deep", mt_keep(deep))), flattened(deep, true));
    check_answers("empty collections contribute nothing", mt_eval(m, E("flatten-deep", mt_keep(hollow))), flattened(hollow, true));

    /* partition reads the verdict. */
    check_answers("partition", mt_eval(m, E("partition", "odd?", mt_keep(four))), partitioned(odd, four));
    check_answers("partition of nothing", mt_eval(m, E("partition", "odd?", mt_unit())), partitioned(odd, none));
    check_answers("a lambda test", mt_eval(m, E("partition", lambda(E(V("x")), E(">", V("x"), 10)), mt_keep(one_two))),
                  partitioned(above_ten, one_two));
    mt_atom *chars = E(T("a"), T("1"));
    check_answers("a deterministic head's False is read",
                  mt_eval(m, E("partition", lambda(E(V("c")), E("unicode-is", V("c"), "letter")), mt_keep(chars))), partitioned(letter, chars));

    /* group-by gathers in first-appearance order. */
    mt_atom *scores = E(80, 20, 90), *shuffled = E(3, 1, 2), *rows = E(E("b", 1), E("a", 1), E("c", 0));
    check_answers("group-by", mt_eval(m, E("group-by", "odd?", mt_keep(four))), only(grouped(odd, four)));
    check_answers("group-by grade", mt_eval(m, E("group-by", "grade", mt_keep(scores))), only(grouped(grade, scores)));
    check_answers("no groups of nothing", mt_eval(m, E("group-by", "odd?", mt_unit())), only(grouped(odd, none)));
    check_answers("sort-by", mt_eval(m, E("sort-by", "double", mt_keep(shuffled))), sorted_by(twice, shuffled));
    check_answers("a stable sort", mt_eval(m, E("sort-by", lambda(E(V("p")), E("index-atom", V("p"), 1)), mt_keep(rows))),
                  sorted_by(second, rows));

    /* scan answers the running results; unfold grows a seed. */
    mt_atom *start = mt_num(0);
    check_answers("scan", mt_eval(m, E("scan", "+", 0, mt_keep(nums))), only(scanned(sum, start, nums)));
    check_answers("scan of nothing is the start", mt_eval(m, E("scan", "+", 0, mt_unit())), only(scanned(sum, start, none)));
    check_answers("scan with a lambda",
                  mt_eval(m, E("scan", lambda(E(V("acc"), V("x")), E("cons-atom", V("x"), V("acc"))), mt_unit(), mt_keep(one_two))),
                  only(scanned(cons_onto, none, one_two)));
    mt_atom *first_seed = mt_num(1);
    check_answers("unfold",
                  mt_eval(m, E("unfold", lambda(E(V("n")), E("if", E("<", V("n"), 4), E(V("n"), E("+", V("n"), 1)), E("empty"))), 1)),
                  only(unfolded(count_to_four, first_seed)));
    check_answers("a step answering nothing", mt_eval(m, E("unfold", lambda(E(V("n")), E("empty")), 1)),
                  only(unfolded(nothing, first_seed)));

    /* pipe reads left to right; apply-to makes a call of a collection. */
    function *const just_double[] = { twice }, *const just_odd[] = { odd }, *const increments[] = { plus_one, plus_two },
                    *const identity[] = { same };
    const stage twice_twice[] = { { just_double, 1 }, { just_double, 1 } }, twice_odd[] = { { just_double, 1 }, { just_odd, 1 } };
    mt_atom *three = mt_num(3);
    check_answers("pipe", mt_eval(m, E("pipe", E("double", "double"), 3)), only(piped(twice_twice, 2, three)));
    check_answers("an empty pipe", mt_eval(m, E("pipe", mt_unit(), 3)), only(piped(NULL, 0, three)));
    check_answers("a pipe into a test", mt_eval(m, E("pipe", E("double", "odd?"), 3)), only(piped(twice_odd, 2, three)));
    mt_atom *five_only = E(5);
    check_answers("apply-to", mt_eval(m, E("apply-to", "+", mt_keep(one_two))), only(applied_to(sum, one_two)));
    check_answers("apply-to one argument", mt_eval(m, E("apply-to", "double", mt_keep(five_only))), only(applied_to(twice, five_only)));

    /* The control forms, beside C's own loop and if. */
    mt_atom *out[MOST];
    check_answers_("repeat", mt_eval(m, E("repeat", 3, E("+", 3, 4))), repeated(3, 3 + 4, out), out);
    check_answers_("repeat zero times", mt_eval(m, E("repeat", 0, E("+", 3, 4))), repeated(0, 3 + 4, out), out);
    check_answers_("unless False", mt_eval(m, E("unless", B(false), E("+", 1, 1))), unless_(false, mt_num(1 + 1), out), out);
    check_answers_("unless True", mt_eval(m, E("unless", B(true), E("+", 1, 1))), unless_(true, mt_num(1 + 1), out), out);
    check_none("while False runs its body no times", mt_eval(m, E("while", B(false), "never")));

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
    check_answers_("while advances the counter", mt_eval(m, E("while", E("<", E("ticks"), 3), E("tick"))), loops, each_tick);
    check_answers("and leaves it at the bound", mt_eval(m, E("ticks")), counter);
    mt_space_close(ticks);

    /* lib_patrick's four. compose reads right to left. */
    require("import lib_patrick", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_patrick")))));
    const stage odd_after_double[] = { { just_double, 1 }, { just_odd, 1 } };
    check_answers("compose", mt_eval(m, E("compose", E("double", "double"), E(3))), only(piped(twice_twice, 2, three)));
    check_answers("compose right to left", mt_eval(m, E("compose", E("odd?", "double"), E(3))), only(piped(odd_after_double, 2, three)));
    int64_t bound = 1 + 2;
    check_answers("@ binds and answers", mt_eval(m, E("@", V("x"), E("+", 1, 2))), bound);
    int64_t tens[3];
    for (int64_t x = 1; x <= 3; x++) tens[x - 1] = x * 10;
    check_answers("for", mt_eval(m, E("for", V("x"), mt_keep(nums), E("*", V("x"), 10))), tens[0], tens[1], tens[2]);
    int64_t state = 0;
    for (int64_t i = 0; i < 3; i++) state += i;
    check_answers("iterate", mt_eval(m, E("iterate", 0, 3, 0, lambda(E(V("i"), V("s")), E("+", V("s"), V("i"))))), state);

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
    check_answers("a branching fold", mt_eval(m, collapsed(E("scan", "branch-add", 0, mt_keep(one_two)))), scanned(branch_add, start, one_two));
    check_answers("a branching unfold", mt_eval(m, collapsed(E("unfold", "branch-step", 0))), unfolded(branch_step, seed));
    check_answers("a branching key", mt_eval(m, collapsed(E("group-by", "branch-key", mt_keep(zero_one)))), grouped(branch_key, zero_one));
    const stage branching[] = { { increments, 2 }, { increments, 2 } };
    check_answers("a branching pipe", mt_eval(m, collapsed(E("pipe", E(E("increment-function"), E("increment-function")), 0))),
                  piped(branching, 2, seed));
    mt_atom *one_arg = E(1), *incremented[4];
    size_t k = 0;
    for (size_t f = 0; f < 2; f++) {
        mt_atom *answers = applied_to(increments[f], one_arg);
        for (size_t i = 0; i < mt_len(answers); i++) incremented[k++] = mt_keep(mt_at(answers, i));
        mt_drop(answers);
    }
    check_answers("apply-to a branching function", mt_eval(m, collapsed(E("apply-to", E("increment-function"), mt_keep(one_arg)))),
                  mt_exprv(k, incremented));
    check_answers("some True answer is Yes", mt_eval(m, E("partition", lambda(E(V("x")), E("superpose", E(B(false), B(true)))), mt_keep(one_two))),
                  partitioned(both_ways, one_two));
    check_answers("no answer is No", mt_eval(m, E("partition", lambda(E(V("x")), E("empty")), mt_keep(one_two))), partitioned(nothing, one_two));
    check_answers("a fold with no answer has no history",
                  mt_eval(m, collapsed(E("scan", lambda(E(V("a"), V("b")), E("empty")), 0, E(1)))), scanned(nothing, start, one_arg));
    check_answers("a step must answer (Value NextSeed)",
                  mt_eval(m, guarded(E("unfold", lambda(E(V("n")), E("if", E("==", V("n"), 0), E(1, 2, 3), E("empty"))), 0))),
                  verdict(computed(unfolded(three_wide, seed))));

    /* Literal values, Error-headed data among them, stay data. */
    mt_atom *arith = E("+", 1, 2), *error = E("Error", "a", "b"), *literal = E(mt_keep(arith), mt_keep(error)),
            *heads = E(E("+", "a"), E("Error", "b")), *wrapped = E(E(mt_keep(arith)), mt_keep(error)),
            *with_three = E(mt_keep(arith), mt_keep(error), 3), *just_arith = E(mt_keep(arith)), *a = S("a");
    check_answers("zip keeps data", mt_eval(m, E("zip", E("quote", mt_keep(literal)), mt_keep(ab))), zipped(literal, ab));
    check_answers("unzip keeps data", mt_eval(m, E("unzip", E("quote", mt_keep(heads)))), unzipped(heads));
    check_answers("flatten-once keeps data", mt_eval(m, E("flatten-once", E("quote", mt_keep(wrapped)))), flattened(wrapped, false));
    check_answers("flatten-deep keeps data", mt_eval(m, E("flatten-deep", E("quote", mt_keep(wrapped)))), flattened(wrapped, true));
    check_answers("partition keeps data",
                  mt_eval(m, E("partition", lambda(E(V("x")), E("==", E("get-metatype", V("x")), "Expression")), E("quote", mt_keep(with_three)))),
                  partitioned(is_expression, with_three));
    check_answers("group-by keeps data", mt_eval(m, E("group-by", lambda(E(V("x")), E("get-metatype", V("x"))), E("quote", mt_keep(literal)))),
                  only(grouped(metatype, literal)));
    check_answers("sort-by keeps data", mt_eval(m, E("sort-by", lambda(E(V("x")), mt_num(0)), E("quote", mt_keep(literal)))), sorted_by(zero, literal));
    check_answers("scan keeps data",
                  mt_eval(m, E("scan", lambda(E(V("acc"), V("item")), E("quote", V("item"))), "a", E("quote", mt_keep(literal)))),
                  only(scanned(latest, a, literal)));
    check_answers("chunk keeps data", mt_eval(m, E("chunk", E("quote", mt_keep(literal)), 1)), pieces(literal, 1, 1, true));
    check_answers("window keeps data", mt_eval(m, E("window", E("quote", mt_keep(literal)), 2)), pieces(literal, 2, 1, false));
    check_answers("apply-to keeps data",
                  mt_eval(m, E("apply-to", lambda(E(V("data")), E("quote", V("data"))), E("quote", mt_keep(just_arith)))),
                  only(applied_to(same, just_arith)));
    const stage identity_stage[] = { { identity, 1 } };
    check_answers("pipe keeps data", mt_eval(m, E("pipe", E(lambda(E(V("data")), E("quote", V("data")))), E("quote", mt_keep(arith)))),
                  only(piped(identity_stage, 1, arith)));

    mt_atom *held[] = { none, just_a, nums, abc, ab, pairs, one_two, lone, four, five, nested, inner, deep, hollow, chars, scores, shuffled, rows,
                        start, first_seed, three, five_only, zero_one, seed, one_arg, arith, error, literal, heads, wrapped,
                        with_three, just_arith, a };
    for (size_t i = 0; i < sizeof held / sizeof *held; i++) mt_drop(held[i]);
    return done(m);
}
