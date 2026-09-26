/* Purpose: a C generator. mt_iterator is next() and close() over state the
 *   program owns; mt_answers_from() makes it a cursor like any the engine
 *   answers, which yields a value per step and closes the state exactly once,
 *   whether the stream ran out or was abandoned.
 * Owns resources: each producer's state, released by close().
 * Guarantees: 0 1 2 then MT_DONE, and close runs once on exhaustion and once
 *   on abandonment [tested 2026-09-27T00:35:58+10:00:
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

typedef struct counter { int64_t next, stop; unsigned *closed; } counter;

static mt_status count_up(void *state, mt_atom **out)
{
    counter *c = state;
    if (c->next == c->stop) return MT_DONE;
    *out = N(c->next++);
    return *out ? MT_ROW : mt_error();
}

static void close_counter(void *state)
{
    counter *c = state;
    ++*c->closed;
    free(c);
}

static mt_answers *range(int64_t stop, unsigned *closed)
{
    counter *c = malloc(sizeof *c);
    require("allocate a counter", c != NULL);
    *c = (counter){ 0, stop, closed };
    return mt_answers_from((mt_iterator){ c, count_up, close_counter });
}

int main(void)
{
    unsigned closed = 0;
    mt_answers *three = range(3, &closed);
    const mt_atom *value;
    for (int64_t i = 0; i < 3; i++) {
        require("a row", mt_step(three, &value) == MT_ROW);
        assert(mt_int(value) == i && "the counter's next value");
    }
    assert(mt_step(three, &value) == MT_DONE && value == NULL && "then exhaustion, told apart from a row");
    mt_answers_free(three);
    assert(closed == 1 && "exhaustion closes the counter once");

    mt_answers *endless = range(1000000, &closed);
    require("one row", mt_step(endless, &value) == MT_ROW);
    mt_answers_free(endless);
    assert(closed == 2 && "abandoning it closes it once too");
    return 0;
}
