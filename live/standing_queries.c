/* Purpose: Observe committed changes and suppress speculative notifications.
 * Owns resources: local handles and host storage are released before success;
 *   a failed assertion terminates this example process.
 * Guarantees: results are asserted [tested: make check; commit=6022c3f48b6dc64752c6e49cfe9d985c7ac7a4e9].
 * Open Obligations: None.
 */
#include "common.h"
typedef struct { unsigned adds, removes; } events;
static mt_status changed(void *user, bool added, const mt_atom *atom)
{
    events *e = user;
    check_atom("event payload", atom, "(item 7)");
    if (added) ++e->adds; else ++e->removes;
    return MT_OK;
}
static mt_status insert(metta *m, void *user)
{ (void)user; return mt_add(m, mt_expr("item", 7)) ? MT_OK : mt_error(); }
int main(void)
{
    metta *m = open_engine(); events e = {0};
    check("subscribe", mt_subscribe(m, "items", (mt_subscription){
        .space="&self", .pattern=mt_expr("item", mt_var("x")), .notify=changed, .user=&e}));
    check("speculate", mt_speculate(m, insert, NULL) == MT_OK);
    check("no speculative event", e.adds == 0 && mt_count(m) == 0);
    check("commit", mt_transaction(m, insert, NULL) == MT_OK);
    check("one committed add", e.adds == 1);
    check("remove item", mt_del(m, mt_expr("item", 7)));
    check("one committed removal", e.removes == 1);
    check("unsubscribe", mt_unsubscribe(m, "items"));
    check("write after unsubscribe", mt_add(m, mt_expr("item", 7)));
    check("no late delivery", e.adds == 1);
    return done(m, "standing_queries");
}
