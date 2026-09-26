/* Purpose: a write a later match can see. matchtrickery is a C function
 *   the engine calls: it adds (foo a) and (foo b) to the space the call runs
 *   in, then streams (bar x) for every (foo x) that space now holds, reading
 *   its own writes through a cursor it hands back as an mt_iterator.
 * Guarantees: (matchtrickery) answers (bar a) then (bar b)
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
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("publish matchtrickery", mt_def(m, (mt_op){ .name = "matchtrickery", .arity = 0,
        .effect = MT_EFFECT_CLASS_WRITES_STATE, .fn = matchtrickery }));
    assert(answers_are(mt_eval(m, E("matchtrickery")), E(E("bar", "a"), E("bar", "b")))
           && "the call sees the facts it wrote");
    mt_close(m);
    return 0;
}
