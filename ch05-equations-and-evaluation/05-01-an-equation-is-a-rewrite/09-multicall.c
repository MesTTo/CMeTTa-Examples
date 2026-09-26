/* Purpose: one head, two answers. mycalc is a C generator: the engine calls
 *   it once and it answers x + y, then x - y, from an mt_iterator the engine
 *   pulls, the C spelling of two equations for one head.
 * Guarantees: (mycalc 1 2) answers 3 then -1 [tested 2026-09-27T00:35:58+10:00:
 *   make -C extensions/cmetta corpus-check].
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
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("publish mycalc", mt_def(m, (mt_op){ .name = "mycalc", .arity = 2,
                                                 .effect = MT_EFFECT_CLASS_NONDETERMINISTIC_READ_ONLY, .fn = mycalc }));
    assert(answers_are(mt_eval(m, E("mycalc", 1, 2)), E(3, -1)) && "both alternatives answer");
    mt_close(m);
    return 0;
}
