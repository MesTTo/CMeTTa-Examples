/* Purpose: Stream owned C values and close on exhaustion or abandonment.
 * Owns resources: local handles and host storage are released before success;
 *   a failed assertion terminates this example process.
 * Guarantees: results are asserted [tested: make check; commit=WORKTREE].
 * Open Obligations: None.
 */
#include "common.h"
typedef struct { int next; int stop; unsigned *closed; } producer;
static mt_status next_value(void *user, mt_atom **out)
{
    producer *p = user;
    if (p->next == p->stop) return MT_DONE;
    *out = mt_num(p->next++);
    return *out ? MT_ROW : mt_error();
}
static void close_values(void *user)
{ producer *p = user; ++*p->closed; free(p); }
static mt_answers *range(int stop, unsigned *closed)
{
    producer *p = malloc(sizeof(*p));
    check("allocate producer", p != NULL);
    *p = (producer){0, stop, closed};
    return mt_answers_from((mt_iterator){p, next_value, close_values});
}
int main(void)
{
    unsigned closed = 0;
    mt_answers *all = range(3, &closed);
    const mt_atom *answer;
    for (int i = 0; i < 3; ++i) {
        check("stream row", mt_step(all, &answer) == MT_ROW);
        check("stream value", mt_int(answer) == i);
    }
    check("explicit exhaustion", mt_step(all, &answer) == MT_DONE && answer == NULL);
    mt_answers_free(all);
    check("exhaustion closes once", closed == 1);
    mt_answers *partial = range(100, &closed);
    check("one row before cancellation", mt_step(partial, &answer) == MT_ROW);
    mt_answers_free(partial);
    check("abandonment closes once", closed == 2);
    return done(NULL, "native_iterator");
}
