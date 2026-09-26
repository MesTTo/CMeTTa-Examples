/* Purpose: three set operations with the comparison passed in. lib_roman's
 *   /?\, \? and \?/ take a sameness as their first argument, and so do C's:
 *   an intersection, a subtraction and a union over arrays, each handed a
 *   comparison as a function pointer. close-enough is a C function the
 *   engine calls and C's operations call directly, and passing == back
 *   reproduces the fixed spellings. fst, snd and cns read pairs; traceid
 *   and tracem print and answer their subject unchanged.
 * Guarantees: all seventeen claims of the original hold
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
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_roman", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_roman")))));
    require("(: close-enough (-> Number Number Bool))",
            mt_add(m, E(":", "close-enough", E("->", "Number", "Number", "Bool"))));
    require("publish close-enough", mt_def(m, (mt_op){ .name = "close-enough", .arity = 2, .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL,
                                                     .fn = close_enough }));

    const set left = { { 1, 5, 9 }, 3 }, right = { { 2, 100 }, 2 }, empty = { .n = 0 };
    assert(answers_are(mt_eval(m, E("/?\\", "close-enough", atom_of(left), atom_of(right))), E(atom_of(filtered(close_enough_, &left, &right, true))))
           && "/?\\ keeps what is related");
    assert(answers_are(mt_eval(m, E("/?\\", "close-enough", atom_of(left), atom_of(empty))), E(atom_of(filtered(close_enough_, &left, &empty, true))))
           && "to nothing, nothing");
    assert(answers_are(mt_eval(m, E("\\?", "close-enough", atom_of(left), atom_of(right))), E(atom_of(filtered(close_enough_, &left, &right, false))))
           && "\\? keeps what is not");
    assert(answers_are(mt_eval(m, E("\\?", "close-enough", atom_of(left), atom_of(empty))), E(atom_of(filtered(close_enough_, &left, &empty, false))))
           && "from nothing, everything");
    assert(answers_are(mt_eval(m, E("\\?/", "close-enough", atom_of(left), atom_of(right))), E(atom_of(united(close_enough_, &left, &right))))
           && "\\?/ is that, then the right");
    assert(answers_are(mt_eval(m, E("\\?/", "close-enough", atom_of(empty), atom_of(right))), E(atom_of(united(close_enough_, &empty, &right))))
           && "with nothing on the left");

    mt_atom *a = E(1, 2, 3), *b = E(2, 3, 4);
    assert(answers_are(mt_eval(m, E("==", E("/?\\", "==", mt_keep(a), mt_keep(b)), E("/==\\", mt_keep(a), mt_keep(b)))), E(B(true))) && "== reproduces /==\\");
    assert(answers_are(mt_eval(m, E("==", E("\\?", "==", mt_keep(a), mt_keep(b)), E("\\==", mt_keep(a), mt_keep(b)))), E(B(true))) && "and \\==");
    assert(answers_are(mt_eval(m, E("==", E("\\?/", "==", mt_keep(a), mt_keep(b)), E("\\==/", mt_keep(a), mt_keep(b)))), E(B(true))) && "and \\==/");
    mt_drop(a);
    mt_drop(b);

    assert(answers_are(mt_eval(m, E("fst", E("a", "b"))), E("a")) && "fst");
    assert(answers_are(mt_eval(m, E("snd", E("a", "b"))), E("b")) && "snd");
    assert(answers_are(mt_eval(m, E("cns", E(1, E(2, 3)))), E(E(1, 2, 3))) && "cns");
    assert(answers_are(mt_eval(m, E("cns", E(E("f", 1), mt_unit()))), E(E(E("f", 1)))) && "cns keeps a call as written");
    assert(answers_are(mt_eval(m, E("traceid", 42)), E(42)) && "traceid answers its subject");
    assert(answers_are(mt_eval(m, E("tracem", T("the answer"), 42)), E(42)) && "tracem too");
    assert(answers_are(mt_eval(m, E("+", 1, E("traceid", 41))), E(42)) && "inside an expression");
    mt_close(m);
    return 0;
}
