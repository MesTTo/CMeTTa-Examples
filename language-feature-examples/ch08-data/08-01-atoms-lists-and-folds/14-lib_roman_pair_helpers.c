/* Purpose: pairs, from lib_roman. first and second apply a function to one
 *   side of a pair and flip swaps the sides; inc is a C function, and C does
 *   the same to a struct through a function pointer.
 * Guarantees: all three claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

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
    metta *m = open_engine();
    require("import lib_roman", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_roman")))));
    require("publish inc", mt_def(m, (mt_op){ .name = "inc", .arity = 1, .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = inc }));
    const pair p = { 1, 9 };
    check_answers("first", mt_eval(m, E("first", "inc", atom_of(p))), atom_of(first(inc_, p)));
    check_answers("second", mt_eval(m, E("second", "inc", atom_of(p))), atom_of(second(inc_, p)));
    check_answers("flip", mt_eval(m, E("flip", E("left", "right"))), E("right", "left"));
    return done(m);
}
