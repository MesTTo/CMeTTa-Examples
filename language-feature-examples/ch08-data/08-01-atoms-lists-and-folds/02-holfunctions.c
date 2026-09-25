/* Purpose: the higher-order forms. foldl-atom, map-atom and filter-atom run
 *   once with their work written inline and once handed foldfun, mapfun and
 *   filterfun, which are C functions; C folds, maps and filters the same
 *   arrays with the same functions through function pointers, and each
 *   engine answer must be C's.
 * Guarantees: all six claims of the original hold, and the closing fold of
 *   pairs by append is checked too [tested: make twins; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

static int64_t add(int64_t a, int64_t b) { return a + b; }
static int64_t inc(int64_t a) { return a + 1; }
static bool above3(int64_t x) { return x > 3; }

static mt_status foldfun(mt_call *call, void *user)
{
    (void)user;
    return mt_answer(call, N(add(mt_int(mt_arg(call, 0)), mt_int(mt_arg(call, 1)))));
}
static mt_status mapfun(mt_call *call, void *user)
{
    (void)user;
    return mt_answer(call, N(inc(mt_int(mt_arg(call, 0)))));
}
static mt_status filterfun(mt_call *call, void *user)
{
    (void)user;
    return mt_answer(call, B(above3(mt_int(mt_arg(call, 0)))));
}

static const int64_t FOUR[] = { 1, 2, 3, 4 }, THREE[] = { 1, 2, 3 }, FIVE[] = { 1, 2, 3, 4, 5 };
#define COUNT(a) (sizeof (a) / sizeof *(a))

static int64_t fold(const int64_t *xs, size_t n, int64_t acc, int64_t (*f)(int64_t, int64_t))
{
    for (size_t i = 0; i < n; i++) acc = f(acc, xs[i]);
    return acc;
}
static mt_atom *map(const int64_t *xs, size_t n, int64_t (*f)(int64_t))
{
    mt_atom *out[8];
    for (size_t i = 0; i < n; i++) out[i] = N(f(xs[i]));
    return mt_exprv(n, out);
}
static mt_atom *filter(const int64_t *xs, size_t n, bool (*keep)(int64_t))
{
    mt_atom *out[8];
    size_t k = 0;
    for (size_t i = 0; i < n; i++)
        if (keep(xs[i])) out[k++] = N(xs[i]);
    return mt_exprv(k, out);
}

int main(void)
{
    metta *m = open_engine();
    require("f1a", mt_add(m, E("=", E("f1a"), E("foldl-atom", E(1, 2, 3, 4), 0, V("acc"), V("x"), E("+", V("acc"), V("x"))))));
    require("f2a", mt_add(m, E("=", E("f2a"), E("map-atom", E(1, 2, 3), V("x"), E("+", V("x"), 1)))));
    require("f3a", mt_add(m, E("=", E("f3a"), E("filter-atom", E(1, 2, 3, 4, 5), V("x"), E(">", V("x"), 3)))));
    require("publish foldfun", mt_def(m, (mt_op){ .name = "foldfun", .arity = 2, .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = foldfun }));
    require("publish mapfun", mt_def(m, (mt_op){ .name = "mapfun", .arity = 1, .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = mapfun }));
    require("publish filterfun", mt_def(m, (mt_op){ .name = "filterfun", .arity = 1, .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = filterfun }));
    require("f1b", mt_add(m, E("=", E("f1b"), E("foldl-atom", E(1, 2, 3, 4), 0, "foldfun"))));
    require("f2b", mt_add(m, E("=", E("f2b"), E("map-atom", E(1, 2, 3), "mapfun"))));
    require("f3b", mt_add(m, E("=", E("f3b"), E("filter-atom", E(1, 2, 3, 4, 5), "filterfun"))));
    require("foldfun2", mt_add(m, E("=", E("foldfun2", V("a"), V("b")), E("append", V("a"), V("b")))));

    check_answers("fold, inline", mt_eval(m, E("f1a")), fold(FOUR, COUNT(FOUR), 0, add));
    check_answers("map, inline", mt_eval(m, E("f2a")), map(THREE, COUNT(THREE), inc));
    check_answers("filter, inline", mt_eval(m, E("f3a")), filter(FIVE, COUNT(FIVE), above3));
    check_answers("fold with C's function", mt_eval(m, E("f1b")), fold(FOUR, COUNT(FOUR), 0, add));
    check_answers("map with C's function", mt_eval(m, E("f2b")), map(THREE, COUNT(THREE), inc));
    check_answers("filter with C's function", mt_eval(m, E("f3b")), filter(FIVE, COUNT(FIVE), above3));
    check_answers("folding pairs by append flattens them",
                  mt_eval(m, E("foldl-atom", E(E(1, 2), E(3, 4), E(5, 6)), mt_unit(), V("acc"), V("x"),
                               E("append", V("acc"), V("x")))), E(1, 2, 3, 4, 5, 6));
    return done(m);
}
