/* Purpose: lib_roman end to end. The higher-order forms and composition are
 *   checked against C doing the same through function pointers: a map, a
 *   fold, a recursive walk of nested children, f(g(x)) and a fan-out. The
 *   nine set operations, in unifying, equal and alpha variants, answer as
 *   the original says; a C function answering nothing prunes a fan-out;
 *   and the inverse matchers run through mt_solve, each unknown read by
 *   name.
 * Guarantees: all twenty-six claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>

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
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_roman", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_roman")))));
    require("publish mfail", mt_def(m, (mt_op){ .name = "mfail", .arity = 1, .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = mfail }));

    mt_atom *flat = E(1, 2, 3), *nested = E(1, E(2, 3));
    assert(answers_are(mt_eval(m, E("map-flat", E("+", 1), mt_keep(flat))), E(map_nested(flat, inc))) && "map-flat");
    assert(answers_are(mt_eval(m, E("map-nested", E("+", 1), mt_keep(nested))), E(map_nested(nested, inc))) && "map-nested");
    assert(answers_are(mt_eval(m, E("fold-flat", "+", 0, mt_keep(flat))), E(fold_nested(flat, 0, add))) && "fold-flat");
    assert(answers_are(mt_eval(m, E("foldr-flat", "cons", mt_unit(), E(1, E(2, 3), 4))), E(E(1, E(2, 3), 4)))
           && "foldr-flat rebuilds with cons");
    assert(answers_are(mt_eval(m, E("fold-nested", "+", 0, mt_keep(nested))), E(fold_nested(nested, 0, add))) && "fold-nested");

    assert(answers_are(mt_eval(m, E("/=\\", E(1, 2, V("a")), E(2, 3, 4))), E(E(2, 2))) && "/=\\ intersects by unifying");
    assert(answers_are(mt_eval(m, E("/==\\", E(1, 2, 3), E(2, 3, 4))), E(E(2, 3))) && "/==\\ by equality");
    assert(answers_are(mt_eval(m, E("/=a\\", E(1, 2, V("a")), E(2, V("a"), 4))), E(E(2, V("a")))) && "/=a\\ by renaming");
    assert(answers_are(mt_eval(m, E("\\=", E(1, 2, 3), E(V("a"), 3, 4))), E(E(2))) && "\\= subtracts by unifying");
    assert(answers_are(mt_eval(m, E("\\==", E(1, 2, 3), E(2, 3, 4))), E(E(1))) && "\\== by equality");
    assert(answers_are(mt_eval(m, E("\\=a", E(1, 2, V("a")), E(2, V("a"), 4))), E(E(1))) && "\\=a by renaming");
    assert(answers_are(mt_eval(m, E("\\=/", E(1, 2, 3), E(V("a"), 3, 4))), E(E(2, 1, 3, 4))) && "\\=/ unites by unifying");
    assert(answers_are(mt_eval(m, E("\\==/", E(1, 2, 3), E(2, 3, 4))), E(E(1, 2, 3, 4))) && "\\==/ by equality");
    assert(answers_are(mt_eval(m, E("\\=a/", E(1, 2, V("a")), E(2, V("a"), 4))), E(E(1, 2, V("a"), 4))) && "\\=a/ by renaming");

    assert(answers_are(mt_eval(m, E(".", E("+", 1), E("*", 2), 1)), E(inc(twice(1)))) && ". is f(g(x))");
    assert(answers_are(mt_eval(m, E(".:", E("+", 1), "+", 2, 3)), E(inc(add(2, 3)))) && ".: feeds two arguments");
    assert(answers_are(mt_eval(m, E("&&&", E("+", 2), E("*", 2), 1)), E(E(plus2(1), twice(1)))) && "&&& fans out");
    assert(answers_are(mt_eval(m, E("&^&", E("+", 1), E("mfail"), 1)), E(inc(1))) && "a branch answering nothing prunes");

    assert(atom_is(SOLVED(mt_solve(m, E("@", V("lst"), E("cons", V("h"), V("t"))), E(1, 2, 3)), "lst", "h", "t"), E(E(1, 2, 3), 1, E(2, 3)))
           && "@ names the whole and its parts");
    assert(atom_is(SOLVED(mt_solve(m, E("head", V("x")), E(1, 2, 3)), "x"), N(1)) && "head");
    assert(atom_is(SOLVED(mt_solve(m, E("tail", V("xs")), E(1, 2, 3)), "xs"), E(2, 3)) && "tail");
    assert(atom_is(SOLVED(mt_solve(m, E("mylast", V("x")), E(1, 2, 3)), "x"), N(3)) && "mylast");
    assert(atom_is(SOLVED(mt_solve(m, E("init", V("xs")), E(1, 2, 3)), "xs"), E(1, 2)) && "init");
    assert(atom_is(SOLVED(mt_solve(m, E("rcons", V("xs"), V("x")), E(1, 2, 3)), "xs", "x"), E(E(1, 2), 3)) && "rcons");

    assert(answers_are(mt_eval(m, E("prog1", E("+", 1, 1), E("+", 2, 2))), E(2)) && "prog1");
    assert(answers_are(mt_eval(m, E("progn", E("+", 1, 1), E("+", 2, 2))), E(4)) && "progn");
    mt_drop(flat);
    mt_drop(nested);
    mt_close(m);
    return 0;
}
