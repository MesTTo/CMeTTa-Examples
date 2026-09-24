/* Purpose: a write a later match can see. matchtrickery is a C function
 *   the engine calls: it adds (foo a) and (foo b) to the space the call runs
 *   in, then streams (bar x) for every (foo x) that space now holds, reading
 *   its own writes through a cursor it hands back as an mt_iterator.
 * Guarantees: (matchtrickery) answers (bar a) then (bar b) [tested: make
 *   twins; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

/* The iterator's state: the cursor over (foo $x), read one answer a step. */
static mt_status next_bar(void *state, mt_atom **out)
{
    const mt_atom *foo;
    mt_status status = mt_step(state, &foo);
    *out = status == MT_ROW ? E("bar", mt_keep(mt_at(foo, 1))) : NULL;
    return status;
}

static void close_bars(void *state)
{
    mt_answers_free(state);
}

static mt_status matchtrickery(mt_call *call, void *user)
{
    (void)user;
    metta *m = mt_of(call);
    if (!mt_add(m, E("foo", "a")) || !mt_add(m, E("foo", "b"))) return mt_error();
    mt_answers *foos = mt_match(m, E("foo", V("x")));
    if (!foos) return mt_error();
    return mt_answer_iter(call, (mt_iterator){ foos, next_bar, close_bars });
}

int main(void)
{
    metta *m = open_engine();
    require("publish matchtrickery", mt_def(m, (mt_op){ .name = "matchtrickery", .arity = 0,
        .effect = MT_EFFECT_CLASS_WRITES_STATE, .fn = matchtrickery }));
    check_answers("the call sees the facts it wrote",
                  mt_eval(m, E("matchtrickery")), E("bar", "a"), E("bar", "b"));
    return done(m);
}
