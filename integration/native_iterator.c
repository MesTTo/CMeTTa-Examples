/* Purpose: a C generator. mt_iterator is next() and close() over state the
 *   program owns; mt_answers_from() makes it a cursor like any the engine
 *   answers, which yields a value per step and closes the state exactly once,
 *   whether the stream ran out or was abandoned.
 * Owns resources: each producer's state, released by close().
 * Guarantees: 0 1 2 then MT_DONE, and close runs once on exhaustion and once
 *   on abandonment [tested: make check; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

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
        check_int("the counter's next value", mt_int(value), i);
    }
    check("then exhaustion, told apart from a row", mt_step(three, &value) == MT_DONE && value == NULL);
    mt_answers_free(three);
    check_int("exhaustion closes the counter once", closed, 1);

    mt_answers *endless = range(1000000, &closed);
    require("one row", mt_step(endless, &value) == MT_ROW);
    mt_answers_free(endless);
    check_int("abandoning it closes it once too", closed, 2);
    return done(NULL);
}
