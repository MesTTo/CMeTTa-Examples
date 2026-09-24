/* Purpose: lib_roman end to end. The higher-order forms and composition are
 *   checked against C doing the same through function pointers: a map, a
 *   fold, a recursive walk of nested children, f(g(x)) and a fan-out. The
 *   nine set operations, in unifying, equal and alpha variants, answer as
 *   the original says; a C function answering nothing prunes a fan-out;
 *   and the inverse matchers run through mt_solve, each unknown read by
 *   name.
 * Guarantees: all twenty-six claims of the original hold [tested: make
 *   twins; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

static int64_t inc(int64_t x) { return x + 1; }
static int64_t twice(int64_t x) { return x * 2; }
static int64_t plus2(int64_t x) { return x + 2; }
static int64_t add(int64_t a, int64_t b) { return a + b; }

/* A nested list with f applied to every number in it. */
static mt_atom *map_nested(const mt_atom *tree, int64_t (*f)(int64_t))
{
    if (mt_kind_of(tree) != MT_EXPR) return N(f(mt_int(tree)));
    mt_atom *kids[8];
    for (size_t i = 0; i < mt_len(tree); i++) kids[i] = map_nested(mt_at(tree, i), f);
    return mt_exprv(mt_len(tree), kids);
}

/* Every number in a nested list, folded with f. */
static int64_t fold_nested(const mt_atom *tree, int64_t acc, int64_t (*f)(int64_t, int64_t))
{
    if (mt_kind_of(tree) != MT_EXPR) return f(acc, mt_int(tree));
    for (size_t i = 0; i < mt_len(tree); i++) acc = fold_nested(mt_at(tree, i), acc, f);
    return acc;
}

static mt_status mfail(mt_call *call, void *user)
{
    (void)call;
    (void)user;
    return MT_FAIL;
}

/* The one solve row's bindings of `names`, as an expression. */
static mt_atom *solved(mt_answers *rows, size_t n, const char *const *names)
{
    mt_atom *out = NULL;
    mt_rows (row, rows) {
        mt_atom *kids[4];
        for (size_t i = 0; i < n; i++) kids[i] = mt_keep(mt_bound(row, names[i]));
        mt_drop(out);
        out = n == 1 ? kids[0] : mt_exprv(n, kids);
    }
    return out;
}
#define SOLVED(rows, ...) solved((rows), sizeof ((const char *[]){ __VA_ARGS__ }) / sizeof (const char *), \
                                 (const char *[]){ __VA_ARGS__ })

int main(void)
{
    metta *m = open_engine();
    require("import lib_roman", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_roman")))));
    require("publish mfail", mt_def(m, (mt_op){ .name = "mfail", .arity = 1, .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = mfail }));

    mt_atom *flat = E(1, 2, 3), *nested = E(1, E(2, 3));
    check_answers("map-flat", mt_eval(m, E("map-flat", E("+", 1), mt_keep(flat))), map_nested(flat, inc));
    check_answers("map-nested", mt_eval(m, E("map-nested", E("+", 1), mt_keep(nested))), map_nested(nested, inc));
    check_answers("fold-flat", mt_eval(m, E("fold-flat", "+", 0, mt_keep(flat))), fold_nested(flat, 0, add));
    check_answers("foldr-flat rebuilds with cons", mt_eval(m, E("foldr-flat", "cons", mt_unit(), E(1, E(2, 3), 4))),
                  E(1, E(2, 3), 4));
    check_answers("fold-nested", mt_eval(m, E("fold-nested", "+", 0, mt_keep(nested))), fold_nested(nested, 0, add));

    check_answers("/=\\ intersects by unifying", mt_eval(m, E("/=\\", E(1, 2, V("a")), E(2, 3, 4))), E(2, 2));
    check_answers("/==\\ by equality", mt_eval(m, E("/==\\", E(1, 2, 3), E(2, 3, 4))), E(2, 3));
    check_answers("/=a\\ by renaming", mt_eval(m, E("/=a\\", E(1, 2, V("a")), E(2, V("a"), 4))), E(2, V("a")));
    check_answers("\\= subtracts by unifying", mt_eval(m, E("\\=", E(1, 2, 3), E(V("a"), 3, 4))), E(2));
    check_answers("\\== by equality", mt_eval(m, E("\\==", E(1, 2, 3), E(2, 3, 4))), E(1));
    check_answers("\\=a by renaming", mt_eval(m, E("\\=a", E(1, 2, V("a")), E(2, V("a"), 4))), E(1));
    check_answers("\\=/ unites by unifying", mt_eval(m, E("\\=/", E(1, 2, 3), E(V("a"), 3, 4))), E(2, 1, 3, 4));
    check_answers("\\==/ by equality", mt_eval(m, E("\\==/", E(1, 2, 3), E(2, 3, 4))), E(1, 2, 3, 4));
    check_answers("\\=a/ by renaming", mt_eval(m, E("\\=a/", E(1, 2, V("a")), E(2, V("a"), 4))), E(1, 2, V("a"), 4));

    check_answers(". is f(g(x))", mt_eval(m, E(".", E("+", 1), E("*", 2), 1)), inc(twice(1)));
    check_answers(".: feeds two arguments", mt_eval(m, E(".:", E("+", 1), "+", 2, 3)), inc(add(2, 3)));
    check_answers("&&& fans out", mt_eval(m, E("&&&", E("+", 2), E("*", 2), 1)), E(plus2(1), twice(1)));
    check_answers("a branch answering nothing prunes", mt_eval(m, E("&^&", E("+", 1), E("mfail"), 1)), inc(1));

    check_atom("@ names the whole and its parts",
               SOLVED(mt_solve(m, E("@", V("lst"), E("cons", V("h"), V("t"))), E(1, 2, 3)), "lst", "h", "t"),
               E(E(1, 2, 3), 1, E(2, 3)));
    check_atom("head", SOLVED(mt_solve(m, E("head", V("x")), E(1, 2, 3)), "x"), N(1));
    check_atom("tail", SOLVED(mt_solve(m, E("tail", V("xs")), E(1, 2, 3)), "xs"), E(2, 3));
    check_atom("mylast", SOLVED(mt_solve(m, E("mylast", V("x")), E(1, 2, 3)), "x"), N(3));
    check_atom("init", SOLVED(mt_solve(m, E("init", V("xs")), E(1, 2, 3)), "xs"), E(1, 2));
    check_atom("rcons", SOLVED(mt_solve(m, E("rcons", V("xs"), V("x")), E(1, 2, 3)), "xs", "x"), E(E(1, 2), 3));

    check_answers("prog1", mt_eval(m, E("prog1", E("+", 1, 1), E("+", 2, 2))), 2);
    check_answers("progn", mt_eval(m, E("progn", E("+", 1, 1), E("+", 2, 2))), 4);
    mt_drop(flat);
    mt_drop(nested);
    return done(m);
}
