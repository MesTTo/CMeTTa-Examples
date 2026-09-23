/* Purpose: Watch, peek and consume one tuple in an application-owned space.
 * Owns resources: local handles and host storage are released before success;
 *   a failed assertion terminates this example process.
 * Guarantees: results are asserted [tested: make check; commit=WORKTREE].
 * Open Obligations: None.
 */
#include "common.h"
static mt_status observed(void *user, bool added, const mt_atom *atom)
{
    if (added) { ++*(unsigned *)user; check_atom("published job", atom, "(Job 7)"); }
    return MT_OK;
}
int main(void)
{
    metta *m = open_engine(); unsigned events = 0;
    check("Linda operations", mt_do(m, "!(import! &self (library lib_thread))"));
    mt_space *mailbox = mt_space_open(m, "&mailbox"); check("open mailbox", mailbox != NULL);
    check("watch jobs", mt_subscribe(m, "jobs", (mt_subscription){.space="&mailbox",
          .pattern=mt_expr("Job", mt_var("id")), .notify=observed, .user=&events}));
    check("publish job", mt_add(mailbox, mt_expr("Job", 7)));
    check("watch saw commit", events == 1);
    mt_atom *peek = mt_one(mt_eval(m, mt_expr("peek-atom", mt_spaceref("&mailbox"), mt_expr("Job", mt_var("id")), 1.0)));
    check_atom("peeked job", peek, "(Job 7)");
    check("peek leaves tuple", mt_count(mailbox) == 1);
    mt_drop(peek);
    check_answers("atomic take returns occurrence", mt_eval(m,
        mt_expr("take-atom", mt_spaceref("&mailbox"), mt_expr("Job", mt_var("id")), 1.0)), "(Job 7)");
    check("tuple consumed", mt_count(mailbox) == 0);
    check("close watch", mt_unsubscribe(m, "jobs"));
    check("drop mailbox", mt_space_drop(mailbox)); mt_space_close(mailbox);
    return done(m, "linda_coordination");
}
