/* Purpose: Deliver a finite ping-pong exchange through reentrant subscriptions.
 * Owns resources: local handles and host storage are released before success;
 *   a failed assertion terminates this example process.
 * Guarantees: results are asserted [tested: make check; commit=WORKTREE].
 * Open Obligations: None.
 */
#include "common.h"
typedef struct { metta *runtime; int transcript[5]; size_t count; } mailbox;
static mt_status receive(void *user, bool added, const mt_atom *atom)
{
    mailbox *box = user;
    if (!added) return MT_OK;
    int n = (int)mt_int(mt_at(atom, 1));
    bool ping = strcmp(mt_name(mt_at(atom, 0)), "ping") == 0;
    check("transcript capacity follows protocol", box->count < 5);
    box->transcript[box->count++] = 10 * n + (ping ? 0 : 1);
    if (ping && n == 3) return MT_OK;
    return mt_add(box->runtime, mt_expr(ping ? "pong" : "ping", ping ? n : n+1)) ? MT_OK : mt_error();
}
int main(void)
{
    metta *m = open_engine(); mailbox box = {.runtime=m};
    const char *names[] = {"ping", "pong"};
    for (size_t i = 0; i < 2; ++i)
        check("subscribe actor", mt_subscribe(m, names[i], (mt_subscription){
            .space="&self", .pattern=mt_expr(names[i], mt_var("n")), .notify=receive, .user=&box}));
    check("start exchange", mt_add(m, mt_expr("ping", 1)));
    const int expected[] = {10,11,20,21,30};
    check("complete exchange", box.count == 5 && memcmp(box.transcript, expected, sizeof(expected)) == 0);
    for (size_t i = 0; i < 2; ++i) check("cancel actor", mt_unsubscribe(m, names[i]));
    return done(m, "reentrant_events");
}
