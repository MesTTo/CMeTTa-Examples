/* Purpose: a standing query sees commits and nothing else. A write made
 *   inside mt_speculate() is discarded and delivers no event, the same write
 *   inside mt_transaction() commits and delivers one, a removal delivers one,
 *   and after unsubscribing nothing is delivered.
 * Guarantees: one add and one remove are seen, in that order
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

/* The same question as a truth value, for a condition with more in it:
   borrows the atom the program holds and takes the expectation. */
static inline bool alpha_equal(const mt_atom *got, mt_atom *want)
{
    bool equal = got && want && mt_alpha_eq(got, want);
    mt_drop(want);
    return equal;
}

typedef struct seen { unsigned adds, removes; bool other; } seen;

static mt_status changed(void *user, bool added, const mt_atom *item)
{
    seen *s = user;
    if (!alpha_equal(item, E("item", 7))) s->other = true;
    if (added) s->adds++;
    else s->removes++;
    return MT_OK;
}

static mt_status insert(metta *m, void *user)
{
    (void)user;
    return mt_add(m, E("item", 7)) ? MT_OK : mt_error();
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    seen s = {0};
    require("watch items", mt_subscribe(m, "items", (mt_subscription){
        .space = "&self", .pattern = E("item", V("x")), .notify = changed, .user = &s }));

    require("speculate a write", mt_speculate(m, insert, NULL) == MT_OK);
    assert(s.adds == 0 && mt_count(m) == 0
           && "a speculative write raises no event and leaves nothing");
    require("commit the write", mt_transaction(m, insert, NULL) == MT_OK);
    assert(s.adds == 1 && "a committed write raises one event");
    require("remove the item", mt_del(m, E("item", 7)));
    assert(s.removes == 1 && "a removal raises one");

    require("stop watching", mt_unsubscribe(m, "items"));
    require("write again", mt_add(m, E("item", 7)));
    assert(s.adds == 1 && !s.other && "nothing is delivered after unsubscribing");
    mt_close(m);
    return 0;
}
