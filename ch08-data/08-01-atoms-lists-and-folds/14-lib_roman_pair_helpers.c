/* Purpose: pairs, from lib_roman. first and second apply a function to one
 *   side of a pair and flip swaps the sides; inc is a C function, and C does
 *   the same to a struct through a function pointer.
 * Guarantees: all three claims of the original hold
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

typedef struct { int64_t a, b; } pair;
static int64_t inc_(int64_t x) { return x + 1; }
static pair first(int64_t (*f)(int64_t), pair p) { return (pair){ f(p.a), p.b }; }
static pair second(int64_t (*f)(int64_t), pair p) { return (pair){ p.a, f(p.b) }; }
static mt_atom *atom_of(pair p) { return E(p.a, p.b); }

static mt_status inc(mt_call *call, void *user)
{
    (void)user;
    return mt_answer(call, N(inc_(mt_int(mt_arg(call, 0)))));
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_roman", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_roman")))));
    require("publish inc", mt_def(m, (mt_op){ .name = "inc", .arity = 1, .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = inc }));
    const pair p = { 1, 9 };
    assert(answers_are(mt_eval(m, E("first", "inc", atom_of(p))), E(atom_of(first(inc_, p)))) && "first");
    assert(answers_are(mt_eval(m, E("second", "inc", atom_of(p))), E(atom_of(second(inc_, p)))) && "second");
    assert(answers_are(mt_eval(m, E("flip", E("left", "right"))), E(E("right", "left"))) && "flip");
    mt_close(m);
    return 0;
}
