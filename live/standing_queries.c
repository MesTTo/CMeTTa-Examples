/* Purpose: a standing query sees commits and nothing else. A write made
 *   inside mt_speculate() is discarded and delivers no event, the same write
 *   inside mt_transaction() commits and delivers one, a removal delivers one,
 *   and after unsubscribing nothing is delivered.
 * Guarantees: one add and one remove are seen, in that order [tested: make
 *   check; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

typedef struct seen { unsigned adds, removes; bool other; } seen;

static mt_status changed(void *user, bool added, const mt_atom *item)
{
    seen *s = user;
    if (!mt_alpha_eq(item, E("item", 7))) s->other = true;
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
    metta *m = open_engine();
    seen s = {0};
    require("watch items", mt_subscribe(m, "items", (mt_subscription){
        .space = "&self", .pattern = E("item", V("x")), .notify = changed, .user = &s }));

    require("speculate a write", mt_speculate(m, insert, NULL) == MT_OK);
    check("a speculative write raises no event and leaves nothing",
          s.adds == 0 && mt_count(m) == 0);
    require("commit the write", mt_transaction(m, insert, NULL) == MT_OK);
    check_int("a committed write raises one event", s.adds, 1);
    require("remove the item", mt_del(m, E("item", 7)));
    check_int("a removal raises one", s.removes, 1);

    require("stop watching", mt_unsubscribe(m, "items"));
    require("write again", mt_add(m, E("item", 7)));
    check("nothing is delivered after unsubscribing", s.adds == 1 && !s.other);
    return done(m);
}
