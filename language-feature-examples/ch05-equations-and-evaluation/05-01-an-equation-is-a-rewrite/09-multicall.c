/* Purpose: one head, two answers. mycalc is a C generator: the engine calls
 *   it once and it answers x + y, then x - y, from an mt_iterator the engine
 *   pulls, the C spelling of two equations for one head.
 * Guarantees: (mycalc 1 2) answers 3 then -1 [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

typedef struct both { int64_t x, y; int step; } both;

static mt_status next_answer(void *state, mt_atom **out)
{
    both *b = state;
    switch (b->step++) {
    case 0: *out = N(b->x + b->y); return MT_ROW;
    case 1: *out = N(b->x - b->y); return MT_ROW;
    default: *out = NULL; return MT_DONE;
    }
}

static mt_status mycalc(mt_call *call, void *user)
{
    (void)user;
    mt_clear();
    int64_t x = mt_int(mt_arg(call, 0)), y = mt_int(mt_arg(call, 1));
    if (!mt_ok()) return mt_fail(call, "mycalc wants two integers");
    both *b = malloc(sizeof *b);
    if (!b) return mt_fail(call, "out of memory");
    *b = (both){ x, y, 0 };
    return mt_answer_iter(call, (mt_iterator){ b, next_answer, free });
}

int main(void)
{
    metta *m = open_engine();
    require("publish mycalc", mt_def(m, (mt_op){ .name = "mycalc", .arity = 2,
                                                 .effect = MT_EFFECT_CLASS_NONDETERMINISTIC_READ_ONLY, .fn = mycalc }));
    check_answers("both alternatives answer", mt_eval(m, E("mycalc", 1, 2)), 3, -1);
    return done(m);
}
