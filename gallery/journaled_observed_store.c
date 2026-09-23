/* Purpose: commit observed facts into SQLite WAL and reopen durable rows.
 * Owns resources: closes providers before removing the temporary database.
 * Decides: SQLite owns journal recovery; notifications observe committed changes.
 * Guarantees: replay preserves committed rows [tested: make check; commit=6022c3f48b6dc64752c6e49cfe9d985c7ac7a4e9].
 * Open Obligations: None.
 */
#include "support/sqlite_store.h"
#include <unistd.h>
static mt_status pair(metta *m, void *user)
{
    (void)m; mt_space *space = user;
    return mt_add(space, mt_expr("Order", 1, 25)) && mt_add(space, mt_expr("Order", 2, 40)) ? MT_OK : mt_error();
}
static mt_status notice(void *user, bool added, const mt_atom *atom)
{ if (added) { check("order event", mt_len(atom) == 3); ++*(size_t *)user; } return MT_OK; }
int main(void)
{
    char path[] = "ai-tmp/orders-XXXXXX";
    int fd = mkstemp(path); check("temporary database", fd >= 0 && close(fd) == 0);
    metta *m = open_engine(); size_t released = 0, events = 0;
    sql_store *store = sql_open(m, "&orders", path, &released);
    check("WAL mode", sql_exec(store, "PRAGMA journal_mode=WAL; PRAGMA synchronous=FULL") == MT_OK);
    mt_space *space = mt_space_open(m, "&orders"); check("open store", space != NULL);
    check("declare observed writes", mt_add(mt_catalog(m), mt_parse("(events &orders per-write-exactly ordered)")));
    check("subscribe", mt_subscribe(m, "orders", (mt_subscription){.space="&orders",
        .pattern=mt_parse("(Order $id $total)"), .notify=notice, .user=&events}));
    check("commit two orders", mt_transaction(m, pair, space) == MT_OK);
    check("two committed events", events == 2);
    check("unsubscribe", mt_unsubscribe(m, "orders"));
    mt_space_close(space); check("close durable provider", mt_provider_close(m, "&orders"));
    sql_open(m, "&reopened", path, &released);
    space = mt_space_open(m, "&reopened");
    check_answers("replayed orders", mt_atoms(space), "(Order 1 25) (Order 2 40)");
    mt_space_close(space); check("close reopened provider", mt_provider_close(m, "&reopened"));
    check("connections released", released == 2);
    check("remove database", unlink(path) == 0);
    return done(m, "journaled_observed_store");
}
