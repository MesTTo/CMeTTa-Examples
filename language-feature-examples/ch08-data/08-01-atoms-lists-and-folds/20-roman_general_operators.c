/* Purpose: three set operations with the comparison passed in. lib_roman's
 *   /?\, \? and \?/ take a sameness as their first argument, and so do C's:
 *   an intersection, a subtraction and a union over arrays, each handed a
 *   comparison as a function pointer. close-enough is a C function the
 *   engine calls and C's operations call directly, and passing == back
 *   reproduces the fixed spellings. fst, snd and cns read pairs; traceid
 *   and tracem print and answer their subject unchanged.
 * Guarantees: all seventeen claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

typedef bool (*same_fn)(int64_t, int64_t);
static bool close_enough_(int64_t a, int64_t b) { return llabs(a - b) < 2; }

static mt_status close_enough(mt_call *call, void *user)
{
    (void)user;
    return mt_answer(call, B(close_enough_(mt_int(mt_arg(call, 0)), mt_int(mt_arg(call, 1)))));
}

typedef struct { int64_t v[8]; size_t n; } set;

static bool related(same_fn same, int64_t x, const set *to)
{
    for (size_t i = 0; i < to->n; i++)
        if (same(x, to->v[i])) return true;
    return false;
}

/* The members of `a` that `same` relates, or does not relate, to `b`. */
static set filtered(same_fn same, const set *a, const set *b, bool keep_related)
{
    set out = { .n = 0 };
    for (size_t i = 0; i < a->n; i++)
        if (related(same, a->v[i], b) == keep_related) out.v[out.n++] = a->v[i];
    return out;
}

static set united(same_fn same, const set *a, const set *b)
{
    set out = filtered(same, a, b, false);
    for (size_t i = 0; i < b->n; i++) out.v[out.n++] = b->v[i];
    return out;
}

static mt_atom *atom_of(set s)
{
    mt_atom *kids[8];
    for (size_t i = 0; i < s.n; i++) kids[i] = N(s.v[i]);
    return mt_exprv(s.n, kids);
}

int main(void)
{
    metta *m = open_engine();
    require("import lib_roman", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_roman")))));
    require("(: close-enough (-> Number Number Bool))",
            mt_add(m, E(":", "close-enough", E("->", "Number", "Number", "Bool"))));
    require("publish close-enough", mt_def(m, (mt_op){ .name = "close-enough", .arity = 2, .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL,
                                                     .fn = close_enough }));

    const set left = { { 1, 5, 9 }, 3 }, right = { { 2, 100 }, 2 }, empty = { .n = 0 };
    check_answers("/?\\ keeps what is related", mt_eval(m, E("/?\\", "close-enough", atom_of(left), atom_of(right))),
                  atom_of(filtered(close_enough_, &left, &right, true)));
    check_answers("to nothing, nothing", mt_eval(m, E("/?\\", "close-enough", atom_of(left), atom_of(empty))),
                  atom_of(filtered(close_enough_, &left, &empty, true)));
    check_answers("\\? keeps what is not", mt_eval(m, E("\\?", "close-enough", atom_of(left), atom_of(right))),
                  atom_of(filtered(close_enough_, &left, &right, false)));
    check_answers("from nothing, everything", mt_eval(m, E("\\?", "close-enough", atom_of(left), atom_of(empty))),
                  atom_of(filtered(close_enough_, &left, &empty, false)));
    check_answers("\\?/ is that, then the right", mt_eval(m, E("\\?/", "close-enough", atom_of(left), atom_of(right))),
                  atom_of(united(close_enough_, &left, &right)));
    check_answers("with nothing on the left", mt_eval(m, E("\\?/", "close-enough", atom_of(empty), atom_of(right))),
                  atom_of(united(close_enough_, &empty, &right)));

    mt_atom *a = E(1, 2, 3), *b = E(2, 3, 4);
    check_answers("== reproduces /==\\", mt_eval(m, E("==", E("/?\\", "==", mt_keep(a), mt_keep(b)), E("/==\\", mt_keep(a), mt_keep(b)))), B(true));
    check_answers("and \\==", mt_eval(m, E("==", E("\\?", "==", mt_keep(a), mt_keep(b)), E("\\==", mt_keep(a), mt_keep(b)))), B(true));
    check_answers("and \\==/", mt_eval(m, E("==", E("\\?/", "==", mt_keep(a), mt_keep(b)), E("\\==/", mt_keep(a), mt_keep(b)))), B(true));
    mt_drop(a);
    mt_drop(b);

    check_answers("fst", mt_eval(m, E("fst", E("a", "b"))), "a");
    check_answers("snd", mt_eval(m, E("snd", E("a", "b"))), "b");
    check_answers("cns", mt_eval(m, E("cns", E(1, E(2, 3)))), E(1, 2, 3));
    check_answers("cns keeps a call as written", mt_eval(m, E("cns", E(E("f", 1), mt_unit()))), E(E("f", 1)));
    check_answers("traceid answers its subject", mt_eval(m, E("traceid", 42)), 42);
    check_answers("tracem too", mt_eval(m, E("tracem", T("the answer"), 42)), 42);
    check_answers("inside an expression", mt_eval(m, E("+", 1, E("traceid", 41))), 42);
    return done(m);
}
